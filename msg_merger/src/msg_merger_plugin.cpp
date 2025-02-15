/*********************************************************************
*
* Author: Yiu-Ming Chung (yiu-ming.chung@tu-dortmund.de)
*
*
* 
*********************************************************************/

#include <msg_merger/msg_merger_plugin.h>
#include <pluginlib/class_list_macros.h>
#include <ros/master.h>

PLUGINLIB_EXPORT_CLASS(msg_merger_plugin::MergerPlugin, msg_merger_base::BaseMerger)

namespace msg_merger_plugin 
{

  void MergerPlugin::initialize(ros::NodeHandle nh)
  {
    if(!initialized_)
    {
      nh_ = nh;

        std::string ns = "";
        {
          using namespace ros;
          ns = this_node::getNamespace();
        }
        
        obst_array_pub_ = nh.advertise<teb_local_planner::ObstacleWithTrajectoryArray>(ns+"/move_base/TebLocalPlannerROS/dynamic_obstacles",1);

        initialized_ = true;
        ROS_DEBUG("msg_merger plugin initialized.");
    }
    else
    {
        ROS_WARN("msg_merger plugin has already been initialized, doing nothing.");
    }
  }

  void MergerPlugin::obstSubCB
  (
    const teb_local_planner::ObstacleWithTrajectory::ConstPtr& obst_msg,
    teb_local_planner::ObstacleWithTrajectory::Ptr& output
  )
  {   
    boost::mutex::scoped_lock l(obst_subs_array_mutex_);
    *output = *obst_msg;
  }


  void MergerPlugin::compute()
  {
    boost::mutex::scoped_lock l(obst_subs_array_mutex_);
    
    ros::master::getTopics(topic_infos_);

    for (int i=0; i<topic_infos_.size(); i++)
    {
      if (topic_infos_.at(i).datatype == "teb_local_planner/ObstacleWithTrajectory")
      {

        std::string ns = "";
        {
          using namespace ros;
          ns = this_node::getNamespace();
        }

        //ignore own trajectory
        if (topic_infos_.at(i).name.find(ns+"/") != std::string::npos) {
          continue;
        }

        bool present = false;

        auto it = obst_subs_array_.begin();
        while (it != obst_subs_array_.end())
        {
          //check if the current query topic is already subscribed
          if (it->first.get()->getTopic() == topic_infos_.at(i).name)
          {
            //this subscribed topic no longer has any publisher, unsub and delete;
            if (it->first.get()->getNumPublishers()==0)
            {
              it->first.get()->shutdown();
              it = obst_subs_array_.erase(it);
            }
            present = true;
            break;
          }
          else
          {
            it++;
          }
        }

        // this topic has no subscriber, create one
        if (!present)
        {

          teb_local_planner::ObstacleWithTrajectory::Ptr msgPtr = boost::make_shared<teb_local_planner::ObstacleWithTrajectory>();

          boost::shared_ptr<ros::Subscriber> subPtr = boost::make_shared<ros::Subscriber>
          (
            nh_.subscribe<teb_local_planner::ObstacleWithTrajectory>
            (
              topic_infos_.at(i).name, 1, boost::bind(&MergerPlugin::obstSubCB, this, _1, msgPtr)
            )
          );


          obst_subs_array_.push_back
          (
            std::pair<boost::shared_ptr< ros::Subscriber >, boost::shared_ptr< teb_local_planner::ObstacleWithTrajectory >>(subPtr,msgPtr)
          );
        }

        //case of subscribed topics not in the list from master: robot physically removed?
       
      }
    }
  }

  void MergerPlugin::compute2()
  {
    obst_array_.obstacles.clear();
    
    boost::mutex::scoped_lock l(obst_subs_array_mutex_);

    
    for (int i=0; i<obst_subs_array_.size(); i++)
    {
      //received message, not empty message
      if (obst_subs_array_.at(i).second.get()->trajectory.size()!=0)
      {
        //remove old obstacles with trajectory

        double last = obst_subs_array_.at(i).second.get()->header.stamp.toSec() + obst_subs_array_.at(i).second.get()->trajectory.end()->time_from_start.toSec();
        if ( /*last<ros::Time::now().toSec() || last<0*/ false ) // always publish
        {
        }
        else
        {
          obst_array_.obstacles.push_back(*obst_subs_array_.at(i).second);
        }
      }
    }

    if (obst_array_.obstacles.size()!=0)
    {
      obst_array_pub_.publish(obst_array_);
    }
  }

}
/** @file Robot.cpp
 * 
 * @author	Claudio Zito (The University Of Birmingham)
 *
 * @version 1.0
 *
 */

#include <Spam/Spam/Robot.h>
#include <Golem/Device/MultiCtrl/MultiCtrl.h>
//#include <Golem/Tools/XMLData.h>
//#include <Golem/Plan/Data.h>
//#include <Golem/Tools/XMLData.h>
//#include <Golem/Tools/Data.h>
#include <Golem/PhysCtrl/Data.h>
#include <Golem/Device/RobotJustin/RobotJustin.h>

#ifdef WIN32
	#pragma warning (push)
	#pragma warning (disable : 4244)
	#pragma warning (disable : 4996)
#endif
#include <pcl/point_types.h>
#include <pcl/kdtree/kdtree_flann.h>
#ifdef WIN32
	#pragma warning (pop)
#endif

#ifdef _GRASP_HITHANDII_DEBUG
#include <Golem/Device/DLR/DLRHitHandII.h>
#endif // _GRASP_HITHANDII_DEBUG

using namespace golem;
using namespace spam;


//------------------------------------------------------------------------------

Robot::Robot(Scene &scene) : grasp::Robot(scene) {
}
	
bool Robot::create(const Desc& desc) {
	grasp::Robot::create(desc); // throws

	this->desc = desc;
	// overwrite the heuristic
	pFTDrivenHeuristic = dynamic_cast<FTDrivenHeuristic*>(&planner->getHeuristic());

	triggeredGuards.clear();
	triggeredGuards.reserve(fLimit.size());

	return true;
}

//------------------------------------------------------------------------------

void Robot::render() {
	grasp::Robot::render();
}

//------------------------------------------------------------------------------

void Robot::findTarget(const golem::Mat34 &trn, const golem::Controller::State &target, golem::Controller::State &cend) {
	// arm chain and joints pointers
	const golem::Chainspace::Index armChain = armInfo.getChains().begin();
	const golem::Configspace::Range armJoints = armInfo.getJoints();
	// Compute a sequence of targets corresponding to the transformed arm end-effector
	GenWorkspaceChainState gwcs;
	controller->chainForwardTransform(target.cpos, gwcs.wpos);
	gwcs.wpos[armChain].multiply(gwcs.wpos[armChain], controller->getChains()[armChain]->getReferencePose()); // 1:1
	gwcs.wpos[armChain].multiply(trn, gwcs.wpos[armChain]); // new waypoint frame
	gwcs.t = target.t;

	cend = target;
	{
		// lock controller
		golem::CriticalSectionWrapper csw(csController);
		// Find initial target position
		if (!planner->findTarget(target, gwcs, cend))
			throw Message(Message::LEVEL_ERROR, "spam::Robot::findTarget(): Unable to find initial target configuration");
	}
	// update arm configurations and compute average error
	grasp::RBDist err;
	WorkspaceChainCoord wcc;
	controller->chainForwardTransform(cend.cpos, wcc);
	wcc[armChain].multiply(wcc[armChain], controller->getChains()[armChain]->getReferencePose());
	err.add(err, grasp::RBDist(grasp::RBCoord(wcc[armChain]), grasp::RBCoord(gwcs.wpos[armChain])));
	context.write("spam::Robot::findTarget(): Pose error: lin=%.9f, ang=%.9f\n", err.lin, err.ang);

}

//------------------------------------------------------------------------------

//int Robot::checkGuards(std::vector<int> &triggeredGuards, golem::Controller::State &state) {
//	//golem::RobotJustin *justin = getRobotJustin();
//	//if (justin) 
//	//	return justin->getTriggeredGuards(triggeredGuards, state);
//	//else {
//		//const ptrdiff_t forceOffset = hand->getReservedOffset(Controller::RESERVED_INDEX_FORCE_TORQUE);
//	
//		//context.write("spam::Robot::getTriggeredGuards(): retrieving forces from state\n");
//		//grasp::RealSeq force;
//		//force.assign(handInfo.getJoints().size(), REAL_ZERO);
//		//if (forceOffset != Controller::ReservedOffset::UNAVAILABLE) {
//		//	for (Configspace::Index j = handInfo.getJoints().begin(); j < handInfo.getJoints().end(); ++j) {
//		//		const size_t k = j - handInfo.getJoints().begin();
//		//		const size_t z = j - armInfo.getJoints().begin();
//		//		force[k] = state.get<ConfigspaceCoord>(forceOffset)[j];
//		//		if (Math::abs(state.get<ConfigspaceCoord>(forceOffset)[j]) > ftHandGuards[k]) {
//		//			context.write("hand joint %d force=%f guard=%f\n", k, force[k], ftHandGuards[k]);
//		//			triggeredGuards.push_back(z);
//		//		}
//		//	}
//		//}
//		//return triggeredGuards.size();
//	//}
//	return triggeredGuards.empty() ? -1 : triggeredGuards.size();
//
//}
//
//int Robot::getTriggeredGuards(grasp::FTGuard::Seq &triggeredJoints, golem::Controller::State &state) {	
//	triggeredJoints.clear();
//	triggeredJoints.reserve(ftGuards.size());
//
//	const int NUM_JOINTS_CTRL = handInfo.getJoints(handInfo.getChains().begin()).size() - 1;
//	int ret = 0;
//	// in case justin moves with enable guards
//	golem::RobotJustin *justin = getRobotJustin();
//	if (justin) {
//		std::vector<int> triggeredIndeces;
//		triggeredIndeces.reserve(ftGuards.size());
//		if ((ret = justin->getTriggeredGuards(triggeredIndeces, state)) > 0 /*|| !staticObject*/) {
//			context.write("spam::Robot::getTriggeredGuards(): num=%d triggered joint(s):", triggeredIndeces.size());
//			for (std::vector<int>::const_iterator i = triggeredIndeces.begin(); i != triggeredIndeces.end(); ++i) {
//				triggeredJoints.push_back(ftGuards[*i]);
//				context.write(" <chain=%d joint=%d>", ftGuards[*i].chainIdx, ftGuards[*i].jointIdx);
//			}
//			context.write("\n");
////			return triggeredJoints.size(); // return ret;
//		}
//	}
//	else {
//		// in case robot bham
//		for (grasp::FTGuard::Seq::const_iterator i = triggeredGuards.begin(); i != triggeredGuards.end(); ++i)
//			triggeredJoints.push_back(*i);
//		
//		//const ptrdiff_t forceOffset = hand->getReservedOffset(Controller::RESERVED_INDEX_FORCE_TORQUE);
//	
//		//context.write("spam::Robot::getTriggeredGuards(): retrieving forces from state. triggered joint(s): joints=%d guards=%d\n", handInfo.getJoints().size(), ftGuards.size());
//		//grasp::RealSeq force;
//		//force.assign(handInfo.getJoints().size(), REAL_ZERO);
//		//if (forceOffset != Controller::ReservedOffset::UNAVAILABLE) {
//		//	for (Configspace::Index j = handInfo.getJoints().begin(); j < handInfo.getJoints().end(); ++j) {
//		//		const size_t k = j - handInfo.getJoints().begin();
//		//		force[k] = state.get<ConfigspaceCoord>(forceOffset)[j];
//		//		if (state.get<ConfigspaceCoord>(forceOffset)[j] < ftGuards[2*k].value) {
//		//			context.write("hand chain %d joint %d (k=%d) force=%f guard=%f\n", ftGuards[2*k].chainIdx, ftGuards[2*k].joint, k, force[k], ftGuards[2*k].value);
//		//			triggeredJoints.push_back(ftGuards[2*k]);
//		//		}
//		//		else if (state.get<ConfigspaceCoord>(forceOffset)[j] > ftGuards[2*k+1].value) {
//		//			context.write("hand chain %d joint %d (k=%d) force=%f guard=%f\n", ftGuards[2*k+1].chainIdx, ftGuards[2*k+1].joint, k, force[k], ftGuards[2*k+1].value);
//		//			triggeredJoints.push_back(ftGuards[2*k+1]);
//		//		}
//		//	}
//		//}
//		ret = triggeredJoints.size();
//	}
//	//	if (!triggeredJoints.empty())
//	//		return true;
//	//}
//	context.write("spam::Robot::getTriggeredGuards(): %d triggered guard(s).\n", ret);
//	return ret;
//}
//
//void Robot::readFT(const Controller::State &state, grasp::RealSeq &force) const {
//	context.debug("robot::readFT(): retrieve torques\n");
//	const ptrdiff_t forceOffset = hand->getReservedOffset(Controller::RESERVED_INDEX_FORCE_TORQUE);
//	const Chainspace::Index handChain = state.getInfo().getChains().begin()+1;
//
//	force.assign(handInfo.getJoints().size(), REAL_ZERO);
//	context.debug("forces \n");
//	if (forceOffset != Controller::ReservedOffset::UNAVAILABLE) {
//		for (Configspace::Index j = handInfo.getJoints().begin(); j < handInfo.getJoints().end(); ++j) {
//			const size_t k = j - handInfo.getJoints().begin();
//			force[k] = state.get<ConfigspaceCoord>(forceOffset)[j];
//			context.debug("hand joint %d force=%f guard=%f\n", k, force[k], ftHandGuards[k]);
//		}
//	}
//
//}
//
////size_t Robot::isGrasping(std::vector<golem::Configspace::Index> &triggeredJoints, golem::Controller::State &state) {
////	// in case justin tried to grasp with no guards enabled
////	triggeredJoints.clear();
////	const ptrdiff_t forceOffset = hand->getReservedOffset(Controller::RESERVED_INDEX_FORCE_TORQUE);
////	state = recvState().config;
////	
////	context.write("spam::Robot::isGrasping(): retrieving forces from state (hand joints size %d, guards size %d)\n", handInfo.getJoints().size(), ftHandGuards.size());
////	grasp::RealSeq force;
////	readFT(state, force);
////	force.assign(handInfo.getJoints().size(), REAL_ZERO);
////	if (forceOffset != Controller::ReservedOffset::UNAVAILABLE) {
////		for (Configspace::Index j = handInfo.getJoints().begin(); j < handInfo.getJoints().end(); ++j) {
////			const size_t k = j - handInfo.getJoints().begin();
////			//force[k] = state.get<ConfigspaceCoord>(forceOffset)[j];
////			//if (Math::abs(state.get<ConfigspaceCoord>(forceOffset)[j]) > ftHandGuards[k]) {
////			if (Math::abs(force[k]) > ftHandGuards[k]) {
////				context.write("hand joint %d force=%f guard=%f\n", k, force[k], ftHandGuards[k]);
////				triggeredJoints.push_back(j);
////			}
////		}
////	}
////	context.write("robot:isGrasping: done./n");
////	return triggeredJoints.size();
////
////	// Read forces at the hand
////	//const ptrdiff_t forceOffset = hand->getReservedOffset(Controller::RESERVED_INDEX_FORCE_TORQUE);
////	//const Controller::State s = recvState().config;
////	//
////	//grasp::RealSeq force;
////	//force.assign(s.getInfo().getJoints().size(), REAL_ZERO);
////	//if (forceOffset != Controller::ReservedOffset::UNAVAILABLE) {
////	//	for (Configspace::Index j = s.getInfo().getJoints().begin(); j < s.getInfo().getJoints().end(); ++j) {
////	//		const size_t k = j - s.getInfo().getJoints().begin();
////	//		force[k] = s.get<ConfigspaceCoord>(forceOffset)[j];
////	//	}
////	//}
////	//for (grasp::RealSeq::const_iterator i = force.begin(), j = ftHandGuards.begin(); i != force.end() && j != ftHandGuards.end(); ++i, ++j)
////	//	if (Math::abs(*i) >= *j ) 
////	//		return true;
////	//
////	//return false;
////}
//
//size_t Robot::isGrasping(grasp::FTGuard::Seq &triggeredJoints, golem::Controller::State &state) {
//	// in case justin tried to grasp with no guards enabled
//	triggeredJoints.clear();
//	const ptrdiff_t forceOffset = hand->getReservedOffset(Controller::RESERVED_INDEX_FORCE_TORQUE);
//	state = recvState().config;
//	
//	context.write("spam::Robot::isGrasping(): retrieving forces from state (hand joints size %d, guards size %d)\n", handInfo.getJoints().size(), ftHandGuards.size());
//	grasp::RealSeq force;
//	force.assign(handInfo.getJoints().size(), REAL_ZERO);
//	size_t res = 0; // counts the triggered guards. but leaves triggeredjoint empty to avoid the belief update.
//	if (forceOffset != Controller::ReservedOffset::UNAVAILABLE) {
//		for (Configspace::Index j = handInfo.getJoints().begin(); j < handInfo.getJoints().end(); ++j) {
//			const size_t k = j - handInfo.getJoints().begin();
//			force[k] = state.get<ConfigspaceCoord>(forceOffset)[j];
//			if (Math::abs(state.get<ConfigspaceCoord>(forceOffset)[j]) > ftHandGuards[k]) {
//				context.write("hand joint %d force=%f guard=%f\n", k, force[k], ftHandGuards[k]);
//				res++;
////				triggeredJoints.push_back(j);
//				// TODO generate ftguards for each contact
//			}
//		}
//	}
//
//	return res; //triggeredJoints.size();
//}

//------------------------------------------------------------------------------

grasp::RBDist Robot::trnTrajectory(const golem::Mat34& actionFrame, const golem::Mat34& modelFrame, const golem::Mat34& trn, golem::Controller::State::Seq::const_iterator begin, golem::Controller::State::Seq::const_iterator end, golem::Controller::State::Seq& trajectory) {
	if (begin == end)
		throw Message(Message::LEVEL_ERROR, "Robot::createTrajectory(2): Empty input trajectory");
	// arm chain and joints pointers
	const golem::Chainspace::Index armChain = armInfo.getChains().begin();
	const golem::Configspace::Range armJoints = armInfo.getJoints();
	// Compute a sequence of targets corresponding to the transformed arm end-effector
	GenWorkspaceChainState::Seq seq;
	for (Controller::State::Seq::const_iterator i = begin; i != end; ++i) {
		GenWorkspaceChainState gwcs;
		controller->chainForwardTransform(i->cpos, gwcs.wpos);
		gwcs.wpos[armChain].multiply(gwcs.wpos[armChain], controller->getChains()[armChain]->getReferencePose()); // 1:1
		// define the grasp frame
		Mat34 poseFrameInv, graspFrame, graspFrameInv;
		poseFrameInv.setInverse(gwcs.wpos[armChain]);
		graspFrame.multiply(poseFrameInv, actionFrame * modelFrame);
		graspFrameInv.setInverse(graspFrame);
		gwcs.wpos[armChain].multiply(modelFrame, graspFrameInv);
//		context.write("trnTrajectory(): grasp frame at model <%f %f %f>\n", gwcs.wpos[armChain].p.x, gwcs.wpos[armChain].p.y, gwcs.wpos[armChain].p.z);
		gwcs.wpos[armChain].multiply(trn, gwcs.wpos[armChain]); // new waypoint frame
//		context.write("trnTrajectory(): grasp frame at new query <%f %f %f>\n", gwcs.wpos[armChain].p.x, gwcs.wpos[armChain].p.y, gwcs.wpos[armChain].p.z);
		gwcs.t = i->t;
		seq.push_back(gwcs);
	}
	// planner debug
	//context.verbose("%s\n", plannerDebug(*planner).c_str());
	Controller::State::Seq ctrajectory;
	{
		// lock controller
		golem::CriticalSectionWrapper csw(csController);
		// Find initial target position
		Controller::State cend = *begin;
		// planner debug
		//context.verbose("%s\n", plannerWorkspaceDebug(*planner, &seq[0].wpos).c_str());
		if (!planner->findTarget(*begin, seq[0], cend))
			throw Message(Message::LEVEL_ERROR, "Robot::createTrajectory(2): Unable to find initial target configuration");
		// Find remaining position sequence
		if (seq.size() == 1)
			ctrajectory.push_back(cend);
		else if (seq.size() > 1 && !planner->findLocalTrajectory(cend, ++seq.begin(), seq.end(), ctrajectory, ctrajectory.end()))
			throw Message(Message::LEVEL_ERROR, "Robot::createTrajectory(2): Unable to find trajectory");
	}
	// update arm configurations and compute average error
	grasp::RBDist err;
	Controller::State::Seq::const_iterator j = begin;
	for (size_t i = 0; i < ctrajectory.size(); ++i, ++j) {
		// copy config
		trajectory.push_back(*j);
		trajectory.back().set(armInfo.getJoints().begin(), armInfo.getJoints().end(), ctrajectory[i]);
		// error
		WorkspaceChainCoord wcc;
		controller->chainForwardTransform(trajectory.back().cpos, wcc);
		wcc[armChain].multiply(wcc[armChain], controller->getChains()[armChain]->getReferencePose());
		err.add(err, grasp::RBDist(grasp::RBCoord(wcc[armChain]), grasp::RBCoord(seq[i].wpos[armChain])));
	}
	context.write("Robot::createTrajectory(2): Pose error: lin=%.9f, ang=%.9f\n", err.lin, err.ang);
	return err;
}

//------------------------------------------------------------------------------

void spam::XMLData(Robot::Desc &val, golem::Context* context, golem::XMLContext* xmlcontext, bool create) {
	//	grasp::XMLData((grasp::Robot::Desc&)val, context, xmlcontext, create);
	RagGraphPlanner::Desc* pRagGraphPlanner(new RagGraphPlanner::Desc);
	(golem::Planner::Desc&)*pRagGraphPlanner = *val.physPlannerDesc->pPlannerDesc;
	val.physPlannerDesc->pPlannerDesc.reset(pRagGraphPlanner);
	val.physPlannerDesc->pPlannerDesc = RagGraphPlanner::Desc::load(context, xmlcontext->getContextFirst("planner"));

	FTDrivenHeuristic::Desc* pFTDrivenHeuristic(new FTDrivenHeuristic::Desc);
	(golem::Heuristic::Desc&)*pFTDrivenHeuristic = *val.physPlannerDesc->pPlannerDesc->pHeuristicDesc;
	val.physPlannerDesc->pPlannerDesc->pHeuristicDesc.reset(pFTDrivenHeuristic);
	spam::XMLData((FTDrivenHeuristic::Desc&)*val.physPlannerDesc->pPlannerDesc->pHeuristicDesc, xmlcontext->getContextFirst("rag_planner heuristic"), create);
}

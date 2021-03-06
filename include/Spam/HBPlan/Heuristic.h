//------------------------------------------------------------------------------
//               Simultaneous Perception And Manipulation (SPAM)
//------------------------------------------------------------------------------
//
// Copyright (c) 2010 University of Birmingham.
// All rights reserved.
//
// Intelligence Robot Lab.
// School of Computer Science
// University of Birmingham
// Edgbaston, Brimingham. UK.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy 
// of this software and associated documentation files (the "Software"), to deal 
// with the Software without restriction, including without limitation the 
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or 
// sell copies of the Software, and to permit persons to whom the Software is 
// furnished to do so, subject to the following conditions:
// 
//     * Redistributions of source code must retain the above copyright 
//       notice, this list of conditions and the following disclaimers.
//     * Redistributions in binary form must reproduce the above copyright 
//       notice, this list of conditions and the following disclaimers in 
//       the documentation and/or other materials provided with the 
//       distribution.
//     * Neither the names of the Simultaneous Perception And Manipulation 
//		 (SPAM), University of Birmingham, nor the names of its contributors  
//       may be used to endorse or promote products derived from this Software 
//       without specific prior written permission.
//
// The software is provided "as is", without warranty of any kind, express or 
// implied, including but not limited to the warranties of merchantability, 
// fitness for a particular purpose and noninfringement.  In no event shall the 
// contributors or copyright holders be liable for any claim, damages or other 
// liability, whether in an action of contract, tort or otherwise, arising 
// from, out of or in connection with the software or the use of other dealings 
// with the software.
//
//------------------------------------------------------------------------------
//! @Author:   Claudio Zito
//! @Date:     27/10/2012
//------------------------------------------------------------------------------
#pragma once
#ifndef _SPAM_SPAM_HEURISTIC_H_
#define _SPAM_SPAM_HEURISTIC_H_

//------------------------------------------------------------------------------

#include <Golem/Ctrl/SingleCtrl/Data.h>
#include <Spam/HBPlan/Belief.h>

//------------------------------------------------------------------------------

namespace spam {

//------------------------------------------------------------------------------

/** Performance monitor */
//#define _HEURISTIC_PERFMON

//------------------------------------------------------------------------------

/** Exceptions */
MESSAGE_DEF(MsgSpamHeuristic, golem::Message)
MESSAGE_DEF(MsgIncompatibleVectors, golem::Message)

//------------------------------------------------------------------------------

class FTDrivenHeuristic : public golem::Heuristic {
public:
	typedef golem::shared_ptr<FTDrivenHeuristic> Ptr;
	friend class Desc;
	friend class FTModelDesc;

	/** Observational model for force/torque model of arm/hand robot 
		Describes a cone over the normal of the end effector in which
		the likelihood of sensing a contact is greater than zero.
	*/
	class FTModelDesc {
	public:
		/** Enabled/disables likelihood computation */
		bool enabledLikelihood;
		/** Exponential intrinsic parameter */
		golem::Real lambda;
		/** Observational weight for each joint of the hand */
		grasp::RealSeq jointFac;
		/** Density normaliser */
		golem::Real hitNormFac;

		/** Threashold (max linear dist) of sensing a contact */
		golem::Real distMax;
		/** Threashold (max angle) of sensing contact over orizontal axis */
		golem::Real coneTheta1;
		/** Threashold (max angle) of sensing contact over vertical axis */
		golem::Real coneTheta2;

		/** Mahalanobis distance factor */
		golem::Real mahalanobisDistFac;

		/** k-nearest neighbours */
		int k;

		/** Number of points to evaluate */
		size_t points;

		/** Descriptor */
		FTModelDesc() {
			setToDefault();
		}
		/** Sets parameters to default values */
		void setToDefault() {
			enabledLikelihood = true;
			distMax = golem::Real(0.10); // 10 cm
			coneTheta1 = coneTheta2 = golem::REAL_PI; // 90 degrees
			mahalanobisDistFac = golem::Real(0.01);
			hitNormFac = golem::REAL_ZERO;
			lambda = golem::REAL_ONE;
			k = 50;
			points = 10000;
		}
		/** Checks if the description is valid */
		bool isValid() const {
			return true;
		}
	};

	/** Description file */
	class Desc : public golem::Heuristic::Desc {
	protected:
		/** Creates the object from the description. */
		GRASP_CREATE_FROM_OBJECT_DESC1(FTDrivenHeuristic, golem::Heuristic::Ptr, golem::Controller&)

	public:
		/** FTModel descriptor pointer */
		FTModelDesc ftModelDesc;

		/** Kernel diagonal covariance scale metaparameter */
		grasp::RBCoord covariance;
		/** Covariance determinant scale parameter */
		golem::Real covarianceDet;
		/** Covariance determinant threashold (minimum) */
		golem::Real covarianceDetMin;

		/** Contact factor in belief update */
		golem::Real contactFac;
		/** Non contact factor in belief update */
		golem::Real nonContactFac;

		/** Rewards directions for a confortable grasp (approaching with the palm towards the object) */
		golem::Real directionFac;

		/** Enables/disable using k nearest neighbours */
		bool knearest;
		/** Enables/disables use of trimesh */
		bool trimesh;
		/** Number of indeces used to estimate the closest surface */
		size_t numIndeces;
		/** Number of points in used to create the KD-trees (subsample of model points) */
		size_t maxSurfacePoints;

		/** Evaluation descritption file */
		Collision::FlannDesc evaluationDesc;

		/** Check description file */
		Collision::FlannDesc checkDesc;

		/** Constructs the description object. */
		Desc() {
			Desc::setToDefault();
		}
		/** Sets the parameters to the default values */
		void setToDefault(){
			Heuristic::Desc::setToDefault();	
			ftModelDesc.setToDefault();
			std::fill(&covariance[0], &covariance[3], golem::Real(0.02)); // Vec3
			std::fill(&covariance[3], &covariance[7], golem::Real(0.005)); // Quat
			covarianceDet = 10;
			covarianceDetMin = 0.00001;
			contactFac = golem::REAL_ONE;
			nonContactFac = golem::REAL_ONE;
			directionFac = golem::Real(0.25);
			numIndeces = 5;
			knearest = true;
			trimesh = false;
			maxSurfacePoints = 10000;
			evaluationDesc.setToDefault();
			checkDesc.setToDefault();
		}
		/** Checks if the description is valid. */
		virtual bool isValid() const {
			if (!Heuristic::Desc::isValid())
				return false;

			if (!ftModelDesc.isValid())
				return false;

			if (!evaluationDesc.isValid() || !checkDesc.isValid())
				return false;

			for (size_t i = 0; i < grasp::RBCoord::N; ++i)
				if (!golem::Math::isPositive(covariance[i]))
					return false;
			return true;
		}
		/** virtual destructor is required */
		virtual ~Desc() {
		}
	};

#ifdef _HEURISTIC_PERFMON
	static golem::U32 perfCollisionWaypoint, perfCollisionPath, perfCollisionGroup, perfCollisionBounds, perfCollisionSegs, perfCollisionPointCloud, perfBoundDist, perfH;

	static void resetLog();
	static void writeLog(golem::Context &context, const char *str);
#endif

	/** Evaluates a single waypoint */
	virtual golem::Real cost(const golem::Waypoint &w, const golem::Waypoint &root, const golem::Waypoint &goal) const;
	/** Objective cost function of a path between specified waypoints */
	virtual golem::Real cost(const golem::Waypoint &w0, const  golem::Waypoint &w1) const;

	/** Sets heuristic description */
	void setDesc(const Desc& desc);
	void setDesc(const Heuristic::Desc& desc);
	/** Current heuristic description */
	inline const Heuristic::Desc& getDesc() const {
		return this->desc;
	}
	inline const Desc& getFTDrivenDesc() const {
		return ftDrivenDesc;
	}

	/** Acquires pose distribution **/
	inline void setBelief(Belief *belief) { pBelief.reset(belief); };

	/** Acquires manipulator */
	inline void setManipulator(grasp::Manipulator *ptr) { manipulator.reset(ptr); pBelief->setManipulator(ptr); /*collision.reset(new Collision(context, *manipulator));*/ };

	/** Collision detection test function for the single waypoint.
	* @param w			waypoint
	* @return			<code>true</code> if a collision has been detected; <code>false</code> otherwise
	*/
	virtual bool collides(const golem::Waypoint &w) const;

	/** Collision detection test function for the single waypoint with thread data.
	* @param w			waypoint
	* @param data		a pointer to the current thread data
	* @return			<code>true</code> if a collision has been detected; <code>false</code> otherwise
	*/
	virtual bool collides(const golem::Waypoint &w, ThreadData* data) const;

	/** Collision detection test function for the single waypoint.
	* @param w0			waypoint
	* @param w1			waypoint
	* @return			<code>true</code> if a collision has been detected; <code>false</code> otherwise
	*/
	virtual bool collides(const golem::Waypoint &w0, const golem::Waypoint &w1) const;

	/** Collision detection test function for the single waypoint with thread data.
	* @param w0			waypoint
	* @param w1			waypoint
	* @param data		a pointer to the current thread data
	* @return			<code>true</code> if a collision has been detected; <code>false</code> otherwise
	*/
	virtual bool collides(const golem::Waypoint &w0, const golem::Waypoint &w1, ThreadData* data) const;

	/** Sets collision checking with the object to be grasped */
	inline void setPointCloudCollision(const bool cloudCollision) { 
		pointCloudCollision = cloudCollision; 

		//if (collision.get()) {
		//	golem::U32 jobs = 1;
		//	const golem::Parallels* parallels = context.getParallels();
		//	if (parallels != NULL) {
		//		jobs = parallels->getNumOfThreads();
		//	}
		//	collision->clearKDTrees();
		//	for (golem::U32 j = 0; j < jobs; ++j)
		//		for (auto i = pBelief->getHypotheses().begin(); i != pBelief->getHypotheses().end(); ++i)
		//			collision->setKDTree((*i)->getCloud(), j);
		//}
	};

	// Gets collision checking with object's points
	inline bool getPointCloudCollision() {
		return pointCloudCollision;
	}

	/** Sets surface points to be checked for collision */
	inline void setNumCollisionPoints(const golem::U32 points) { waypoint.points = points; };
	/** Returns surface points to be checked for collision */
	inline golem::U32 getNumCollisionPoints() { return waypoint.points; };

	/** Mahalanobis distance between rigid bodies a and b */
	golem::Real distance(const grasp::RBCoord &a, const grasp::RBCoord &b, bool enableAng = false) const;

	inline grasp::RBCoord& getCovarianceSqrt() { return sampleProperties.covarianceSqrt; }

	/** test observational model */
	golem::Real testObservations(const grasp::RBCoord &pose, const bool normal = false) const;

	/** virtual destructor */
	virtual ~FTDrivenHeuristic() {}

	/** Enable/disable of incorporating uncertainty into the cost function */
	bool enableUnc;
	/** Pointer to descriptor */
	FTDrivenHeuristic::Desc ftDrivenDesc;

	bool testCollision;
	inline golem::Real getDist2(const golem::Waypoint& w0, const golem::Waypoint& w1) const {
		return golem::Heuristic::getDist(w0, w1);
	};

	/** Returns a pointer to the collision interface */
	Collision* getCollision() { return collision.get(); };

	/** Returns true only if expected collisions are likely to happen */
	inline bool expectedCollisions(const golem::Controller::State& state) const {
		const grasp::Manipulator::Config config = manipulator->getConfig(state);
		return hypothesisBoundsSeq.empty() ? false : intersect(manipulator->getBounds(config.config, config.frame.toMat34()), hypothesisBoundsSeq, false);
	}

	void renderHypothesisCollisionBounds(golem::DebugRenderer& renderer) {
		renderer.setColour(golem::RGBA(golem::U8(255), golem::U8(255), golem::U8(0), golem::U8(127)));
		renderer.setLineWidth(golem::Real(1.0));
		renderer.addWire(hypothesisBoundsSeq.begin(), hypothesisBoundsSeq.end());
	}
	
	/** Reset collision bounds */
	inline void setHypothesisBounds() {
		hypothesisBoundsSeq.clear();
		for (auto i = pBelief->getHypotheses().begin(); i != pBelief->getHypotheses().end(); ++i) {
			golem::Bounds::Seq tmp = (*i)->bounds();
			hypothesisBoundsSeq.insert(hypothesisBoundsSeq.begin(), tmp.begin(), tmp.end());
		}
	}


protected:
	/** Generator of pseudo random numbers */
	golem::Rand rand;

	/** Pose distribution **/
	Belief::Ptr pBelief;

	/** Manipulator pointer */
	grasp::Manipulator::Ptr manipulator;
	/** Collision interface */
	Collision::Ptr collision;
	/** Collision waypoint */
	Collision::Waypoint waypoint;

	/** Collision bound for check for expected collisions */
	golem::Bounds::Seq hypothesisBoundsSeq;

	/** Sampled poses */
//	HypSample::Map samples;
	/** Transformation samples properties */
	golem::SampleProperty<golem::Real, grasp::RBCoord> sampleProperties;
	///** Inverse covariance matrix associated with the samples */
	//grasp::RBCoord covarianceInv;
	///** Squared covariance matrix associated with the samples */
	//grasp::RBCoord covarianceSqrt;
	/** Determinant of covariance matrix associated with the samples */
	golem::Real covarianceDet;

	/** Observational weight for each joint of the hand */
	grasp::RealSeq jointFac;

	/** Model cloud points */
	grasp::Cloud::PointSeq modelPoints;

	/** Hit normalising factor */
	golem::Real hitNormFacInv;

	/** Robot right arm */
	golem::SingleCtrl *arm;
	/** Robot right hand */
	golem::SingleCtrl *hand;

	/** Controller state info */
	golem::Controller::State::Info armInfo;
	/** Controller state info */
	golem::Controller::State::Info handInfo;

	/** Enables/disables collision checking with the object to be grasped */
	bool pointCloudCollision;

	/** Check the approaching direction of the grasp */
	golem::Real directionApproach(const golem::Waypoint &w) const;

	/** Bounded distance between a waypoint and the set of samples */
	golem::Real getBoundedDist(const golem::Waypoint& w) const;
	/** Distance between two waypoints in workspace */
	golem::Real getMahalanobisDist(const golem::Waypoint& w0, const golem::Waypoint& goal) const;
	/** Observational cost function over a trajectory */
	golem::Real expectedObservationCost(const golem::Waypoint &wi, const golem::Waypoint &wj) const;
	/** Computes the distance between future observations */
	golem::Real psi(const golem::Waypoint& wi, const golem::Waypoint& wj, Hypothesis::Seq::const_iterator p) const;
	/** Pair observational function over a trajectory */
//	void h(const golem::Waypoint &wi, const golem::Waypoint &wj, Belief::Hypothesis::Seq::const_iterator p, std::vector<golem::Real> &y) const;
	/** Pair observational function over a trajectory */
	void h(const golem::Waypoint &wi, const golem::Waypoint &wj, std::vector<golem::Real> &y) const;

	/** Evaluate contact with hyptothesis */
	golem::Real evaluate(const Hypothesis::Seq::const_iterator &hypothesis, const golem::Waypoint &w) const;

	/** Estimate contact with hyptothesis */
	inline golem::Real estimate(const Hypothesis::Seq::const_iterator &hypothesis, const golem::Waypoint& w) const {
		const grasp::Manipulator::Config config(w.cpos);
		return (*hypothesis)->estimate(ftDrivenDesc.evaluationDesc, config, ftDrivenDesc.ftModelDesc.distMax);
	}

	/** Penalises configurations which collide with the mean hypothesis */
	golem::Real getCollisionCost(const golem::Waypoint &wi, const golem::Waypoint &wj, Hypothesis::Seq::const_iterator p) const;

	/** Create heuristic from the description */
	bool create(const Desc &desc);
	/** Constructor */
	FTDrivenHeuristic(golem::Controller &controller);
};

//------------------------------------------------------------------------------

void XMLData(FTDrivenHeuristic::FTModelDesc& val, golem::XMLContext* context, bool create = false);

void XMLData(FTDrivenHeuristic::Desc& val, golem::XMLContext* context, bool create = false);

//------------------------------------------------------------------------------

//golem::Real density(const grasp::RBCoord &x, const grasp::RBCoord &mean, const grasp::RBCoord &covariance, const golem::Real bound = golem::Node::COST_INF);

}; // namespace spam
//------------------------------------------------------------------------------
#endif /** _SPAM_SPAM_HEURISTIC_H_ */
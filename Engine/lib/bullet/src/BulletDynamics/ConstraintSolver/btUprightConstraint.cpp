/*

Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2006 Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:
 
1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

*/

#include "btUprightConstraint.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "LinearMath/btTransformUtil.h"
#include <new>
#include "stdio.h"

//!
//!
//!

void btUprightConstraint::solveAngularLimit(
			btUprightConstraintLimit *limit,
            btScalar timeStep, btScalar jacDiagABInv,
            btRigidBody * body0 )
{
	
	// Work out if limit is violated

	limit->m_currentLimitError = 0;
	if ( limit->m_angle < m_loLimit )
	{
		limit->m_currentLimitError = limit->m_angle - m_loLimit;
	}
	if ( limit->m_angle > m_hiLimit )
	{
		limit->m_currentLimitError = limit->m_angle - m_hiLimit;
	}
	if( limit->m_currentLimitError == 0.0f )
	{
		return;
	}

	btScalar targetVelocity       = -m_ERP*limit->m_currentLimitError/(timeStep);
	btScalar maxMotorForce        = m_maxLimitForce;

    maxMotorForce *= timeStep;

    // current velocity difference
    btVector3 angularVelocity       = body0->getAngularVelocity();
    btScalar  axisAngularVelocity   = limit->m_axis.dot( angularVelocity );
 
     // correction velocity
    btScalar motorVelocity          = m_limitSoftness*(targetVelocity  - m_damping*axisAngularVelocity);

    // correction impulse
    btScalar unclippedMotorImpulse = (1+m_bounce)*motorVelocity*jacDiagABInv;

	// clip correction impulse
    btScalar clippedMotorImpulse = unclippedMotorImpulse;

    //todo: should clip against accumulated impulse

    if (unclippedMotorImpulse>0.0f)
    {
        clippedMotorImpulse =  unclippedMotorImpulse > maxMotorForce? maxMotorForce: unclippedMotorImpulse;
    }
    else
    {
        clippedMotorImpulse =  unclippedMotorImpulse < -maxMotorForce ? -maxMotorForce: unclippedMotorImpulse;
    }

	// sort with accumulated impulses
    btScalar      lo = btScalar(-1e30);
    btScalar      hi = btScalar(1e30);

    btScalar oldaccumImpulse = limit->m_accumulatedImpulse;

    btScalar sum = oldaccumImpulse + clippedMotorImpulse;

	limit->m_accumulatedImpulse = sum > hi ? btScalar(0.) : sum < lo ? btScalar(0.) : sum;

    clippedMotorImpulse = limit->m_accumulatedImpulse - oldaccumImpulse;

    btVector3 motorImp = clippedMotorImpulse * limit->m_axis;
    body0->applyTorqueImpulse(motorImp);
}

//!
//!
//!

btUprightConstraint::btUprightConstraint(btRigidBody& rbA, const btTransform& frameInA )
        : btTypedConstraint(D6_CONSTRAINT_TYPE, rbA)
        , m_frameInA(frameInA)

{
      m_ERP                         = 1.0f;
      m_bounce                      = 0.0f;
      m_damping                     = 1.0f;
      m_limitSoftness               = 1.0f;
      m_maxLimitForce               = 3000.0f;
	  setLimit( SIMD_PI * 0.4f );
}
 
//!
//!
//!

void btUprightConstraint::buildJacobian()
{
      btTransform m_worldTransform = m_rbA.getCenterOfMassTransform() * m_frameInA;

      btVector3   upAxis        = m_worldTransform.getBasis().getColumn( 1 );         

	  m_limit[ 0 ].m_axis		= btVector3( 1, 0, 0 );
      m_limit[ 0 ].m_angle		= ( SIMD_PI / 2 ) - btAtan2( upAxis.getY(), upAxis.getZ() );

	  m_limit[ 1 ].m_axis		= btVector3( 0, 0, 1 );
      m_limit[ 1 ].m_angle		= btAtan2( upAxis.getY(), upAxis.getX() ) - ( SIMD_PI / 2 );

	  for ( int i = 0; i < 2; i++ )
	  {
		  if ( m_limit[ i ].m_angle < -SIMD_PI )
				m_limit[ i ].m_angle += 2 * SIMD_PI;
		  if ( m_limit[ i ].m_angle > SIMD_PI )
				m_limit[ i ].m_angle -= 2 * SIMD_PI;

		  new (&m_jacAng[ i ])      btJacobianEntry(  m_limit[ i ].m_axis,
													  m_rbA.getCenterOfMassTransform().getBasis().transpose(),
													  m_rbB.getCenterOfMassTransform().getBasis().transpose(),
													  m_rbA.getInvInertiaDiagLocal(),
													  m_rbB.getInvInertiaDiagLocal());
	  }
}

//!
//!
//!

void btUprightConstraint::solveConstraint(btScalar    timeStep)
{
    m_timeStep = timeStep;

	solveAngularLimit( &m_limit[ 0 ], m_timeStep, btScalar(1.) / m_jacAng[ 0 ].getDiagonal(), &m_rbA );
	solveAngularLimit( &m_limit[ 1 ], m_timeStep, btScalar(1.) / m_jacAng[ 1 ].getDiagonal(), &m_rbA );
}
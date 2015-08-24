#include "spherepolyhedron.h"
#include "math.h"
#include <stdexcept>
#include <assert.h>
#include "io.h"
#include "application.h"

const float PI = acos(-1);
const float PI2 = PI*2;
const float PIo2 = PI/2;

//Phi = around up axis slices.
//Theta = up/down spices.

//Should replace sin/cos/tan with lookup tables with precision n.

//Bottleneck
math::vec2 SpherePolyhedron::cartesianToSpherical(const math::vec3& cartesian)
{
    return math::vec2(atan2(cartesian.x, cartesian.y), asin(cartesian.z));
}

math::vec3 SpherePolyhedron::sphericalToCartesian(const math::vec2& spherical)
{
    const float sinPhi = sin(spherical.x);
    const float cosPhi = cos(spherical.x);
    const float sinTheta = sin(spherical.y);
    const float cosTheta = cos(spherical.y);

    return math::vec3(sinPhi*cosTheta, cosTheta*cosPhi, sinTheta);
}

math::vec3 SpherePolyhedron::getPointAtIndex(int phi, int theta) const
{
    return mVertices[phi*mVerticesThetaCount+theta];
}

float SpherePolyhedron::findDistanceToPolyhedron(const math::vec3& normalisedVector) const
{

    //Get spherical coordinates of vector
    math::vec2 spherical = cartesianToSpherical(normalisedVector);

    //As the phi component can be negative, move it into the [0,2 Pi] range.
    spherical.x = fmod(spherical.x+PI2, PI2);

    //Get theta into into the [0, PI] range
    float thetaWithOffset = spherical.y + PIo2;

    //Vertices to be found:
    math::vec3 v1, v2, v3;

    //If pointing near the poles
    const float piMinusThetaAngle = PI-mThetaAngle;

    if(thetaWithOffset < mThetaAngle || thetaWithOffset > piMinusThetaAngle)
    {
        const int phiIndexi = (int) (spherical.x/mPhiAngle);

        //If north
        if(thetaWithOffset > piMinusThetaAngle)
        {
            v1 = getPointAtIndex(phiIndexi, mVerticesThetaCount-1);
            v2 = getPointAtIndex((phiIndexi+1) % mVerticesPhiCount,  mVerticesThetaCount-1);
            v3 = mVertices[mVertices.size()-2];
        }
        else //if south
        {
            v1 = getPointAtIndex(phiIndexi, 0);
            v2 = getPointAtIndex((phiIndexi+1) % mVerticesPhiCount, 0);
            v3 = mVertices[mVertices.size()-1];
        }
    }
    else //If pointing at a normal quad
    {
        //Account for cutting off the bottom and top triangles
        thetaWithOffset *= (mThetaFaces)/(mThetaFaces-2);

        //Get a quad index from the spherical coordinates.
        const math::vec2f indexf(spherical.x/mPhiAngle, thetaWithOffset/mThetaAngle);
        const math::vec2i indexi(indexf.x, indexf.y);

        //Get vertices common to both triangles
        v1 = getPointAtIndex((indexi.x+1) % mVerticesPhiCount, indexi.y);
        v2 = getPointAtIndex(indexi.x, (indexi.y + mVerticesThetaCount-1) % mVerticesThetaCount);

        //Upper triangle
        if((indexf.y-indexi.y) / mPhiAngle > (indexf.x-indexi.x) / mPhiAngle)
            v3 = getPointAtIndex(indexi.x, indexi.y);
        //Lower triange
        else
            v3 = getPointAtIndex((indexi.x+1) % mVerticesPhiCount, (indexi.y + mVerticesThetaCount-1) % mVerticesThetaCount);
    }

    //v1,v2,v3 are now filled out.

    //Find distance to triangle
    math::vec3 normal = math::cross(v2-v1,v3-v1);
    float vn = math::dot(normalisedVector, normal);
    assert(vn!=0);

    float distance = (math::dot(v1-mCentre, normal)/vn);

    return distance;
}

void SpherePolyhedron::constructMesh()
{
    using namespace math;

    //Unique vertices
    mVerticesPhiCount = mPhiFaces;
    mVerticesThetaCount = mThetaFaces - 1;

    for(int iPhi = 0; iPhi < mPhiFaces; ++iPhi)
        for(int iTheta = 1; iTheta < mVerticesThetaCount+1; ++iTheta)
            mVertices.push_back(sphericalToCartesian(vec2(iPhi*mPhiAngle, iTheta*mThetaAngle-PIo2)));

    //Add in poles
    //North
    mVertices.push_back(math::vec3(0,0,1));
    //South
    mVertices.push_back(math::vec3(0,0,-1));

}

SpherePolyhedron::SpherePolyhedron() :mCentreAndRadiusChanged(false) {}

SpherePolyhedron::SpherePolyhedron(unsigned phiFaces, unsigned thetaFaces)
    : mPhiFaces(phiFaces), mThetaFaces(thetaFaces), mRadius(1),
      mPhiAngle(PI2/phiFaces), mThetaAngle(PI/thetaFaces), mCentreAndRadiusChanged(false)
{
    assert(phiFaces > 3 && thetaFaces > 2);
    constructMesh();
}

float SpherePolyhedron::findIntersection(const math::vec3 normalisedVector) const
{
    if(mVertices.size()==0)
        throw std::runtime_error("Attempting to find ray/triangle intersection with uninitialised polyhedron.");
    return findDistanceToPolyhedron(normalisedVector);
}

void SpherePolyhedron::setCentreAndRadius(const math::vec3 centre, float radius)
{
    if(mCentreAndRadiusChanged)
        Warning("Setting centre and radius a second time on sphere: transformation applied to non-unit starting mesh.");
    mCentreAndRadiusChanged = true;

    for(auto it = mVertices.begin(); it!=mVertices.end(); ++it)
        *it = (*it)*radius + centre;

    mRadius = radius;
    mCentre = centre;
}

void SpherePolyhedron::debugDraw(math::vec3 colour) const
{
    //Iterate over the quads
    glColor3f(colour.x,colour.y,colour.z);
    glPointSize(5.f);
    for(int iPhi = 0; iPhi < mVerticesPhiCount; ++iPhi)
        for(int iTheta = 0; iTheta < mVerticesThetaCount-1; ++iTheta)
        {
            //Get points
            glBegin(GL_POINTS);
            auto v1 = getPointAtIndex(iPhi, iTheta);
            auto v2 = getPointAtIndex(iPhi, iTheta+1);
            auto v3 = getPointAtIndex(((iPhi+1) % mVerticesPhiCount), iTheta+1);
            auto v4 = getPointAtIndex(((iPhi+1) % mVerticesPhiCount), iTheta);

            //Draw points
            glVertex3f(v1.x,v1.y,v1.z);
            glVertex3f(v2.x,v2.y,v2.z);
            glVertex3f(v3.x,v3.y,v3.z);
            glVertex3f(v4.x,v4.y,v4.z);
            glEnd();

            //Draw triangles
            glBegin(GL_LINES);
            glVertex3f(v1.x,v1.y,v1.z);
            glVertex3f(v3.x,v3.y,v3.z);
            glVertex3f(v1.x,v1.y,v1.z);
            glVertex3f(v2.x,v2.y,v2.z);
            glVertex3f(v2.x,v2.y,v2.z);
            glVertex3f(v3.x,v3.y,v3.z);
            glVertex3f(v4.x,v4.y,v4.z);
            glVertex3f(v1.x,v1.y,v1.z);
            glEnd();
        }

    //Draw poles
    math::vec3 north = mVertices[mVertices.size()-2];
    math::vec3 south = mVertices[mVertices.size()-1];
    glBegin(GL_POINTS);
    glVertex3f(north.x,north.y,north.z);
    glVertex3f(south.x,south.y,south.z);
    glEnd();

    //Draw pole triangles
    glBegin(GL_LINES);
    for(int iPhi = 0; iPhi < mPhiFaces; ++iPhi)
    {
        auto edgePointTop1 = getPointAtIndex(iPhi, mVerticesThetaCount-1);
        auto edgePointTop2 = getPointAtIndex((iPhi+1) % (mVerticesPhiCount), mVerticesThetaCount-1);
        glVertex3f(north.x,north.y,north.z);
        glVertex3f(edgePointTop1.x,edgePointTop1.y,edgePointTop1.z);
        glVertex3f(north.x,north.y,north.z);
        glVertex3f(edgePointTop2.x,edgePointTop2.y,edgePointTop2.z);
        glVertex3f(edgePointTop1.x,edgePointTop1.y,edgePointTop1.z);
        glVertex3f(edgePointTop2.x,edgePointTop2.y,edgePointTop2.z);
    }
    for(int iPhi = 0; iPhi < mPhiFaces; ++iPhi)
    {
        auto edgePointTop1 = getPointAtIndex(iPhi, 0);
        auto edgePointTop2 = getPointAtIndex((iPhi+1) % (mVerticesPhiCount), 0);
        glVertex3f(south.x,south.y,south.z);
        glVertex3f(edgePointTop1.x,edgePointTop1.y,edgePointTop1.z);
        glVertex3f(south.x,south.y,south.z);
        glVertex3f(edgePointTop2.x,edgePointTop2.y,edgePointTop2.z);
        glVertex3f(edgePointTop1.x,edgePointTop1.y,edgePointTop1.z);
        glVertex3f(edgePointTop2.x,edgePointTop2.y,edgePointTop2.z);
    }
    glEnd();
}
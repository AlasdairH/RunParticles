

#include "TrackLayer.h"
#include "Types.h"

#include "cinder/gl/gl.h"

using namespace cinder;

TrackLayer::TrackLayer(const Track *track) :
_track(track)
{
    // do nothing
}

TrackLayer::~TrackLayer() 
{
    // do nothing
}

double lerp(double a, double b, double f)
{
    return a + f * (b - a);
}

MapPoint lerp(const MapPoint& a, const MapPoint& b, double f)
{
    return MapPoint(lerp(a.x, b.x, f), lerp(a.y, b.y, f));
}

void 
TrackLayer::draw(const ViewCtx *viewCtx, const TimeCtx *timeCtx)
{
    TrackPoint pt;
    MapPoint lastMapPt;
    TrackPoint lastTrkPt;
    int startTime = 0;
    gl::color( Color( 1, 0, 0 ) );
    for(ulong i=0; i < _track->points.count(); i++) {
        pt = _track->points[i];
        MapPoint thisMapPt = viewCtx->toProjection(pt.pos);
        if (i == 0 || (pt.time - startTime) < timeCtx->getMapSeconds()) {
            if (i > 0) {
                gl::drawLine( Vec2f(lastMapPt.x, lastMapPt.y),
                              Vec2f(thisMapPt.x, thisMapPt.y));
            } else {
                startTime = pt.time;
            }
            lastMapPt = thisMapPt;
            lastTrkPt = pt;
        } else {
            double elapsed = timeCtx->getMapSeconds()
                             - double(lastTrkPt.time - startTime);
            int trkElapsed = pt.time - lastTrkPt.time;
            double f = (trkElapsed == 0) ? 0. : elapsed / double(trkElapsed);
            if (f > 0.0) {
                MapPoint finalPt = lerp(lastMapPt, thisMapPt, f);
                gl::drawLine( Vec2f(lastMapPt.x, lastMapPt.y),
                              Vec2f(finalPt.x, finalPt.y) );
                gl::color( Color( 1, 1, 1 ) );
                gl::drawSolidCircle( Vec2f( finalPt.x, finalPt.y ), 1.0 );
            } else {
                gl::color( Color( 1, 1, 1 ) );
                gl::drawSolidCircle( Vec2f( thisMapPt.x, thisMapPt.y ), 1.0 );
            }
            break;
        }
    }
}

void
TrackLayer::boundingBox(double &left,
                         double &top,
                         double &right,
                         double &bottom)
{
    left = 180.;
    top = -90.;
    right = -180.;
    bottom = 90.;
    TrackPoint pt;
    foreach(pt, _track->points) {
        if (pt.pos.x > right)
            right = pt.pos.x;
        if (pt.pos.x < left)
            left = pt.pos.x;
        if (pt.pos.y > top)
            top = pt.pos.y;
        if (pt.pos.y < bottom)
            bottom = pt.pos.y;
    }
}

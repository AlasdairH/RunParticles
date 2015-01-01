/*
 *  Types.h
 *  RunParticles
 *
 *  Created by Doug Letterman on 2/16/13.
 *  Copyright 2013 Doug Letterman. All rights reserved.
 *
 */

#ifndef TYPES_H
#define TYPES_H

#include <set>
#include <vector>

#include "cinder/Vector.h"

#include <QDateTime>
#include <QList>
#include <QString>

using namespace cinder;

typedef std::set<unsigned int> PassMap;

enum {
    PassLayer_Background,
    PassLayer_MidGround = 50,
    PassLayer_Foreground = 100,
    PassLayer_Count
};

class LonLat : public Vec2d
{
public:
    LonLat() : Vec2d(0., 0.) { };
    LonLat(double x, double y) : Vec2d(x, y) { };
    double lon() const { return x; };
    double lat() const { return y; };
};

typedef Vec2d MapPoint;

struct TrackPoint
{
    LonLat pos;
    unsigned int time;
};

typedef std::vector<TrackPoint> TrackPath;

struct Track
{
    QString sport;
    QString name;
    QString sourceFilePath;
    TrackPath points;
};

struct PathPoint
{
    MapPoint pos;
    unsigned int time;
};

typedef QList<PathPoint> Path;

#endif

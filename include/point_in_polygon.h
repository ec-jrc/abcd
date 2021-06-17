#ifndef __POINT_IN_POLYGON_H__
#define __POINT_IN_POLYGON_H__

#define data_type double

// Copyright 2000 softSurfer, 2012 Dan Sunday
// This code may be freely used and modified for any purpose
// providing that this copyright notice is included with it.
// SoftSurfer makes no warranty for this code, and cannot be held
// liable for any real or imagined damage resulting from its use.
// Users of this code must verify correctness for their application.
 
/******************************************************************************/
/* A point is defined by its coordinates (x, y)                               */
/******************************************************************************/
struct Point_t {
    data_type x;
    data_type y;
};
 
struct BoundingBox_t {
    struct Point_t top_left;
    struct Point_t bottom_right;
};

inline struct BoundingBox_t compute_bounding_box(struct Point_t *polygon, size_t n) {
    struct BoundingBox_t bounding_box;

    if (n <= 0) {
        bounding_box.top_left.x = 0;
        bounding_box.top_left.y = 0;
        bounding_box.bottom_right.x = 0;
        bounding_box.bottom_right.y = 0;
    }
    else
    {
        bounding_box.top_left.x = polygon[0].x;
        bounding_box.top_left.y = polygon[0].y;
        bounding_box.bottom_right.x = polygon[0].x;
        bounding_box.bottom_right.y = polygon[0].y;
        
        for (size_t i = 1; i < n; ++i)
        {
            if (polygon[i].x < bounding_box.top_left.x)
            {
                bounding_box.top_left.x = polygon[i].x;
            }
            if (polygon[i].x > bounding_box.bottom_right.x)
            {
                bounding_box.bottom_right.x = polygon[i].x;
            }
            if (polygon[i].y > bounding_box.top_left.y)
            {
                bounding_box.top_left.y = polygon[i].y;
            }
            if (polygon[i].y < bounding_box.bottom_right.y)
            {
                bounding_box.bottom_right.y = polygon[i].y;
            }
        }
    }

    return bounding_box;
}

inline int point_in_bounding_box(struct Point_t P, struct BoundingBox_t bounding_box)
{
    return (bounding_box.top_left.x <= P.x) && (P.x < bounding_box.bottom_right.x) &&
           (bounding_box.bottom_right.y <= P.y) && (P.y < bounding_box.top_left.y);
}

/******************************************************************************/
// is_left(): tests if a point is Left|On|Right of an infinite line.
//    Input:  three points P0, P1, and P
//    Return: >0 for P left of the line through P0 and P1
//            =0 for P  on the line
//            <0 for P  right of the line
//    See: Algorithm 1 "Area of Triangles and Polygons"
/******************************************************************************/
inline data_type is_left(struct Point_t P0, struct Point_t P1, struct Point_t P)
{
    return (P1.x - P0.x) * (P.y - P0.y) - (P.x -  P0.x) * (P1.y - P0.y);
}

/******************************************************************************/
/* point_in_polygon(): winding number test for a point in a polygon           */
/*                                                                            */
/*  Input:  P: the point,                                                     */
/*          polygon[]:  Vertex points of a polygon                            */
/*                      WARNING: The polygon shall have n+1 points with       */
/*                      polygon[n] = polygon[0]                               */
/*          n: the number of points of the polygon, see warning.              */
/*                                                                            */
/*  Return: wn: the winding number (=0 only when P is outside)                */
/*                                                                            */
/******************************************************************************/
inline int point_winding_number(struct Point_t P, struct Point_t* polygon, size_t n)
{
    int wn = 0;    // the  winding number counter

    // loop through all edges of the polygon
    for (size_t i = 0; i < n; ++i)
    {
        // An edge is from polygon[i] to polygon[i+1]

        // Start with y <= P.y
        if (polygon[i].y <= P.y)
        {
            // An upward crossing
            if (polygon[i + 1].y > P.y)
            {
                // P left of edge
                if (is_left(polygon[i], polygon[i + 1], P) > 0)
                {
                    // Have a valid up intersect
                    ++wn;
                }
            }
        }
        else
        {
            // A downward crossing
            if (polygon[i + 1].y  <= P.y)
            {
                // P right of edge
                if (is_left(polygon[i], polygon[i + 1], P) < 0)
                {
                    // Have a valid down intersect
                    --wn;
                }
            }
        }
    }

    return wn;
}

#endif

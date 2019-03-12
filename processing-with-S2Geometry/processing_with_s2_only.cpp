#include <iostream>
#include "s2/s2cell.h"
#include "s2/s2latlng.h"
#include "s2/s2loop.h"
#include "s2/s2region_coverer.h"
#include <boost/geometry.hpp>
#include <fstream>


namespace bg = boost::geometry;
using namespace std;

//inmemory database :)
vector<S2Point> allPonts;
vector<S2Loop *> s2Loops;
vector<vector<S2Point>> polygonsAsS2Points;
vector<S2LatLngRect> latLngRects;
vector<S2CellUnion> exteriors;
vector<S2CellUnion> interiors;

vector<int> pointsBasic;
vector<int> pointsAdvance;
//end of database

S2LatLng getLatLngFromSting(string line) {
    //TODO clean it
    boost::replace_all(line, "),", "");
    boost::replace_all(line, ")", "");
    boost::replace_all(line, " ", "");

    unsigned long t = line.find(',');
    try {
        auto lng = boost::lexical_cast<double>(line.substr(0, t));

        auto lat = boost::lexical_cast<double>(line.substr(t + 2, line.size() - t - 2));

        return S2LatLng::FromDegrees(lat, lng).Normalized();
    } catch (const std::exception &e) {
        cout << line << endl;
    }

}

S2LatLng getLatLngFromLine(const string &line) {
    unsigned long t;
    if (line.find("POINT") != -1) {
        t = line.find('\"');
        string coords = line.substr(t, -1);
        coords = coords.substr(2, coords.length() - 4);
        return getLatLngFromSting(coords);

    } else {
        return getLatLngFromSting(line);
    }

}


void getPointsFromFile(const string &points_path) {
    ifstream points_stream(points_path);

    static string line;

    while (std::getline(points_stream, line)) {
        S2LatLng latLng = getLatLngFromLine(line);
        S2Point point = latLng.ToPoint();
        allPonts.push_back(point);
    }
}

bool findS2Point(const vector<S2Point> points,const S2Point point) {
    for (auto p : points) {
        if (p == point) {
            return true;
        }
    }
    return false;
}

void indexPolygons(const string &polyAsString) {
    unsigned long t = polyAsString.find(',');
    string coords = polyAsString.substr(t, -1);

    coords = coords.substr(3, coords.length() - 5);
    vector<S2Point> s2Points;
    while (coords.length() > 10) {

        t = coords.find(')');
        string one_coord = coords.substr(2, t - 2);
        //TODO clean
        if (coords.length() < 50) {

            coords = coords;
            one_coord = coords.substr(1, t - 1);

            S2LatLng latLng = getLatLngFromLine(one_coord);

            bool duplicate = findS2Point(s2Points, latLng.ToPoint());
            if (!duplicate) {
                s2Points.push_back(latLng.ToPoint());
            }
            break;
        } else {
            coords = coords.substr(t + 3, -1);
            one_coord = coords.substr(1, t - 2);

            S2LatLng latLng = getLatLngFromLine(one_coord);
            bool duplicate = findS2Point(s2Points, latLng.ToPoint());
            if (!duplicate) {
                s2Points.push_back(latLng.ToPoint());
            }

        }
    }


    polygonsAsS2Points.push_back(s2Points);
    //basic loop
    S2Loop loop;
    loop.Init(s2Points);
    s2Loops.push_back(loop.Clone());
    // Start of extra indexes
    auto start = std::chrono::high_resolution_clock::now();
    //Rect
    S2LatLngRect boundingBox = loop.GetRectBound();
    latLngRects.push_back(boundingBox);
    //Region over polygon
    S2RegionCoverer::Options options;
    options.set_max_level(20);
    options.set_max_cells(128);
    S2RegionCoverer regionCoverer(options);
    S2CellUnion intUnion = regionCoverer.GetInteriorCovering(loop);
    intUnion.Normalize();
    S2CellUnion extUnion = regionCoverer.GetCovering(loop);
    extUnion.Normalize();
    //Save
    exteriors.push_back(extUnion);
    interiors.push_back(intUnion);
    auto stop = std::chrono::high_resolution_clock::now();
    cout << "Time for indexing of S2Rect, Exterior and Interior "
         << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count() << " ms" << endl;

}

void getPolysFromFile(const string &polys_path) {
    ifstream poly_stream(polys_path);

    static string line;
    while (std::getline(poly_stream, line)) {
        indexPolygons(line);
    }
}


void calculateBasic() {
    unsigned long numberOfPolygons = polygonsAsS2Points.size();
    //84
    for (int i = 0; i < numberOfPolygons; i++) {
        int counter = 0;
        for (auto point : allPonts) {

            bool inside = s2Loops[i]->Contains(point);

            if (inside) {
                counter++;
            } else {

            }

        }
        pointsBasic.push_back(counter);
    }
}

void calculateAdvance() {
    unsigned long numberOfPolygons = polygonsAsS2Points.size();

    for (int i = 0; i < numberOfPolygons; i++) {
        int counter = 0;
        for (auto point : allPonts) {
            bool inside = latLngRects[i].Contains(point);
            if (inside) {
                inside = exteriors[i].Contains(point);
                if (inside) {
                    inside = interiors[i].Contains(point);
                    if (inside) {
                        counter++;
                    } else {
                        counter += s2Loops[i]->Contains(point);
                    }
                }
            }
        }
        pointsAdvance.push_back(counter);
    }
}

int main(int argc, char *argv[]) {
    std::string pathToData = argv[1];

    string points_path = pathToData + "/converted_points1000.csv";
    getPointsFromFile(points_path);

    string polys_path = pathToData + "/converted_poly15.csv";

    getPolysFromFile(polys_path);

    auto start = std::chrono::high_resolution_clock::now();
    calculateBasic();
    auto stop = std::chrono::high_resolution_clock::now();
    cout << "Time using only S2Loops: " << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count()
         << " ms"
         << endl;

    start = std::chrono::high_resolution_clock::now();
    calculateAdvance();
    stop = std::chrono::high_resolution_clock::now();
    cout << "Time using S2Rect, Exterior and Interior: "
         << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count() << " ms"
         << endl;

    //Print results
    for (auto i : pointsBasic) {
        cout << i << "\t";
    }
    cout << endl;
    for (auto i : pointsAdvance) {
        cout << i << "\t";
    }
    cout << endl;
    return 0;
}

#include <iostream>
#include <boost/geometry.hpp>
#include <fstream>
#include "s2/s2cell.h"
#include "s2/s2latlng.h"
#include "s2/s2loop.h"
#include "s2/s2region_coverer.h"


namespace bg = boost::geometry;
using namespace std;
using namespace std::chrono;

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


typedef bg::model::point<double, 2, bg::cs::cartesian> BoostPoint;
typedef bg::model::polygon<BoostPoint> BoostPolygon;

S2LatLng getLatLngFromLine(const string &line) {
	BoostPoint point;
	bg::read_wkt(line, point);
	auto lng = point.get<0>();
	auto lat = point.get<1>();
	return S2LatLng::FromDegrees(lat, lng);

}


void getPointsFromFile(const string &points_path) {
	ifstream points_stream(points_path);

	static string line;

	while (std::getline(points_stream, line)) {
		S2LatLng latLng = getLatLngFromLine(line);

		allPonts.push_back(latLng.ToPoint());
	}
}

bool findS2Point(const vector<S2Point> points, const S2Point point) {
	for (auto p : points) {
		if (p == point) {
			return true;
		}
	}
	return false;
}

void indexPolygons(string polyAsString) {
	boost::replace_all(polyAsString, "\"", "");
	BoostPolygon polygon;
	bg::read_wkt(polyAsString, polygon);

	vector<S2Point> s2Points;
	bool l = bg::is_valid(polygon);
	if (!l){
		bg::correct(polygon);
	}

	for (unsigned i = 0; i < polygon.outer().size(); i++) {

		auto lng =  boost::geometry::get<0>(polygon.outer()[i]);
		auto lat =  boost::geometry::get<1>(polygon.outer()[i]);
		S2LatLng latLng  = S2LatLng::FromDegrees(lat, lng);
		S2Point  s2Point = latLng.ToPoint();

		bool duplicate = findS2Point(s2Points, s2Point);
		if (!duplicate) {
			s2Points.push_back(s2Point);
		}

	}

	vector<S2Point> s2PointsReverted;
	for (int i = s2Points.size()-1; i >=0; i--) {
		s2PointsReverted.push_back(s2Points.at(i));
	}

	polygonsAsS2Points.push_back(s2PointsReverted);
//	polygonsAsS2Points.push_back(s2Points);
	//basic loop
	S2Loop loop;
	loop.Init(s2PointsReverted);
	s2Loops.push_back(loop.Clone());
	// Start of extra indexes
//	auto start = std::chrono::high_resolution_clock::now();
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
//	auto stop = std::chrono::high_resolution_clock::now();
//	cout << "Time for indexing of S2Rect, Exterior and Interior "
//		 << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count() << " ms" << endl;

}

void getPolysFromFile(const string &polys_path) {
	ifstream poly_stream(polys_path);

	static string line;
	int c = 0;
	while (std::getline(poly_stream, line)) {
//		cout<<"Polygon id="<<c<<endl;
		indexPolygons(line);
		c++;
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
	if (argv[1] == NULL) {
		cout << "You have to provide path to data with points and polygons as first argument..." << endl
			 << "Example: /home/{HOME}/geo-data-processing/data"
			 << endl;
		return -1;
	}
	std::string pathToData = argv[1];
	string polys_path = pathToData + "/wkt_poly10.txt";
	string points_path = pathToData + "/wkt_points500.txt";

	auto start = high_resolution_clock::now();
	getPolysFromFile(polys_path);
	auto stop = high_resolution_clock::now();
	cout << "Read polygons into S2Loop then build S2RegionCoverer: " << duration_cast<milliseconds>(stop - start).count()<< " ms"<< endl;

	start = high_resolution_clock::now();
	getPointsFromFile(points_path);
	stop = high_resolution_clock::now();
	cout << "Read points into S2LatLng : " << duration_cast<milliseconds>(stop - start).count()<< " ms"<< endl;

	cout << "Number of polygons: " << polygonsAsS2Points.size() << endl;
	cout << "Number of points: " << allPonts.size() << endl;

	start = std::chrono::high_resolution_clock::now();
	calculateBasic();
	stop = std::chrono::high_resolution_clock::now();
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
		cout << i << ",";
	}
	cout << endl;
	for (auto i : pointsAdvance) {
		cout << i << ",";
	}
	cout << endl;
	return 0;
}

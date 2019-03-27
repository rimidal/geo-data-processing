#include <iostream>
#include <boost/geometry.hpp>
#include <fstream>
#include <chrono>
#include "interval_tree/itree.h"

using namespace std;
using namespace std::chrono;

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

typedef bg::model::point<double, 2, bg::cs::cartesian> BoostPoint;
typedef bg::model::polygon<BoostPoint> BoostPolygon;
typedef bg::model::box<BoostPoint> BoostBox;


class PolygonEdge {
public:
	typedef double range_type;
	typedef unsigned int id_type;

	PolygonEdge(range_type rHigh, range_type rLow, unsigned short sID, range_type sx1, range_type sx2)
			: m_rHigh(rHigh > rLow ? rHigh : rLow), m_rLow(rLow <= rHigh ? rLow : rHigh), m_sID(sID), m_x1(sx1),
			  m_x2(sx2) {}

	range_type low() const { return m_rLow; }

	range_type x1() const { return m_x1; }

	range_type x2() const { return m_x2; }

	range_type high() const { return m_rHigh; }

	id_type id() const { return m_sID; }


	range_type m_rHigh, m_rLow, m_x1, m_x2;
	id_type m_sID;
};

typedef std::pair<BoostBox, unsigned> BoxAndPolId;
typedef bgi::rtree<BoxAndPolId, bgi::rstar<16>> RTree;

//inmemory database :)
vector<BoostPoint> boostPoints;
vector<BoostPolygon> boostPolygons;
vector<BoostBox> boostBoxes;
vector<itree<PolygonEdge>> polygonsAsIntervalTree;
RTree rTree;
vector<long> numberOfPointsInside;

BoostPoint getPointFromLine(string line) {
	BoostPoint boostPoint;
	bg::read_wkt(line, boostPoint);
	return boostPoint;
}

void getPointsFromFile(const string &pathToPoints) {
	ifstream points_stream(pathToPoints);

	static string line;
	while (std::getline(points_stream, line)) {
		BoostPoint point = getPointFromLine(line);
		boostPoints.push_back(point);
	}
}

void putPolygonInRtree(BoostPolygon polygon, unsigned id) {
	BoostBox box;
	bg::envelope(polygon, box);

	itree<PolygonEdge> intervalTree;
	//TODO second iteration!!

	for (unsigned i = 0; i < polygon.outer().size(); i++) {
		auto x1 = boost::geometry::get<0>(polygon.outer()[i]);//todo remove cast
		auto x2 = boost::geometry::get<0>(polygon.outer()[i + 1]);
		auto rHigh = boost::geometry::get<1>(polygon.outer()[i]);
		auto rLow = boost::geometry::get<1>(polygon.outer()[i + 1]);

		if (rHigh > rLow) {
			PolygonEdge obs = PolygonEdge(rHigh, rLow, i, x1, x2);
			intervalTree.push_back(obs);

		} else if (rHigh < rLow) {
			PolygonEdge obs = PolygonEdge(rLow, rHigh, i, x1, x2);
			intervalTree.push_back(obs);
		}
	}
	polygonsAsIntervalTree.push_back(intervalTree);

	boostPolygons.push_back(polygon);
	numberOfPointsInside.push_back(0);

	rTree.insert(std::make_pair(box, id));
}


void indexPolygons(string polyAsString, unsigned id) {
	BoostPolygon boostPolygon;
	boost::replace_all(polyAsString, "\"", "");
	bg::read_wkt(polyAsString, boostPolygon);

	putPolygonInRtree(boostPolygon, id);
}


void indexPolygonsFromFile(const string &polys_path) {
	ifstream poly_stream(polys_path);

	static string line;
	unsigned id = 0;
	while (std::getline(poly_stream, line)) {

		indexPolygons(line, id);

		id++;
	}
}

void calculateWithIntervalTree() {
	for (auto point : boostPoints) {

		vector<BoxAndPolId> result_s;
		rTree.query(bgi::intersects(point), std::back_inserter(result_s));
		for (auto &result_ : result_s) {
			auto s = result_.second;
			auto f = result_.first;

			BoostPolygon polygon = boostPolygons.at(s);


			PolygonEdge::range_type queryValue = (double) point.get<1>();

			itree<PolygonEdge>::query_iterator iter = polygonsAsIntervalTree[s].qbegin(queryValue);
			itree<PolygonEdge>::query_iterator iter2 = polygonsAsIntervalTree[s].qend(queryValue);
			int intersect = 0;
			while (iter != iter2) {

				if (iter.cur_node == NULL) {
					int i = 1;
				} else {
					if (point.get<0>() < iter->m_x1 and point.get<0>() < iter->m_x2) {
						intersect++;
					}
				}
				iter++;
			}
			if (intersect % 2 != 0) {
				numberOfPointsInside.at(s) += 1;
			}

		}
	}
}

void calculateWithRtree() {
	for (auto point : boostPoints) {

		vector<BoxAndPolId> result_s;
		rTree.query(bgi::intersects(point), std::back_inserter(result_s));
		for (auto &result_ : result_s) {
			auto s = result_.second;

			BoostPolygon polygon = boostPolygons.at(s);

			bool within = boost::geometry::within(point, polygon);

			if (within) {
				numberOfPointsInside.at(s) += 1;
			}
		}
	}
}

void simpleCalculate() {
	for (auto point : boostPoints) {
		BoostBox boxForPoint;

		for (unsigned i = 0; i < boostPolygons.size(); i++) {
			bool within = boost::geometry::within(point, boostPolygons.at(i));
			if (within) {
				numberOfPointsInside.at(i) += 1;
			}
		}
	}
}

void printResults() {
	for (long &i : numberOfPointsInside) {
		cout << i << ",";
		i = 0;
	}
	cout << endl;

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
	indexPolygonsFromFile(polys_path);
	auto stop = high_resolution_clock::now();
	cout << "Time to read polygons and put into R-Star-Tree: " << duration_cast<milliseconds>(stop - start).count()<< " ms"<< endl;

	start = high_resolution_clock::now();
	getPointsFromFile(points_path);
	stop = high_resolution_clock::now();
	cout << "Time to read points: " << duration_cast<milliseconds>(stop - start).count()<< " ms"<< endl;

	cout << "Number of polygons: " << boostPolygons.size() << endl;
	cout << "Number of points: " << boostPoints.size() << endl;

	start = high_resolution_clock::now();
	for (int i = 0; i < polygonsAsIntervalTree.size(); i++) {
		polygonsAsIntervalTree[i].construct();
	}
	stop = high_resolution_clock::now();
	cout << "Time to build Interval Trees: " << duration_cast<milliseconds>(stop - start).count()<< " ms"<< endl;

	cout << "Using Boost Polygons: ";
	start = high_resolution_clock::now();
	simpleCalculate();
	stop = high_resolution_clock::now();
	cout << duration_cast<milliseconds>(stop - start).count()<< " ms"<< endl;
	printResults();

	cout << "Using R-Star-Tree: ";
	start = high_resolution_clock::now();
	calculateWithRtree();
	stop = high_resolution_clock::now();
	cout << duration_cast<milliseconds>(stop - start).count()<< " ms"<< endl;
	printResults();

	cout << "Using R-Star-Tree and Interval Trees: ";
	start = high_resolution_clock::now();
	calculateWithIntervalTree();
	stop = high_resolution_clock::now();
	cout << duration_cast<milliseconds>(stop - start).count()<< " ms"<< endl;
	printResults();


	return 0;
}
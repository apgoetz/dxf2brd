#include <dxflib/dl_dxf.h>
#include <dxflib/dl_creationadapter.h>
#include <iostream>



// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.


// AUTHOR: Andy Goetz
// MAIL: andy@andygoetz.org
// PROGRAM: dxf2brd

// This program converts DXF files to kicad BRD files.  The KICad
// board designer has very primitive support drawing shapes
// accurately. You can use this program to create a complex design in
// a real cad program, like qcad, and import it into a kicad BRD file. 

// Compiling:
// to use this program, you must have dxflib installed. 
// then use the command:
// 
// g++ dxf2brd.cpp -o dxf2brd -ldxflib
//
// To compile the program. 

// running the program:
// to run the program, execute the following command:
//
// ./dxf2brd some_dxf_file.dxf
//
// It will produce BRD code as its output. to add this code to an existing BRD file, run the following command:
//
// ./dxf2brd some_dxf_file.dxf >> some_brd_file.brd


// these defines control the kicad output. All units are tenths of mils:
#define LINE_THICKNESS 150 	// thickness of traces
#define X_OFFSET 40000 		// x offset of DXF origin in kicad coordinate space
#define Y_OFFSET 40000		// y offset of DXF origin in kicad coordinate space
#define LAYER 28		// layer to render output on


// This class contains the callbacks from dxflib only 3 important ones
// matter: the callbacks for lines, circles, and arcs.
class Dxf2BrdFilter : public DL_CreationAdapter {
	// called when a line has been detected
	virtual void addLine(const DL_LineData& d);
	// called when a circle has been detected
	virtual void addCircle(const DL_CircleData& d);
	// called when an arc has been detected
	virtual void addArc(const DL_ArcData& d);
protected:
	// converts a DXF Coordinate, angle and radius into a KICAD
	// coordinate that is offset by the radius in the direction of
	// angle
	void convertangle(double xin, double yin, double radius, double angle, int &xout, int &yout);
	// converts a DXF coordinate to a KICAD coordinate
	void convert(double xin, double yin, int &xout, int &yout);
	// offset (in 10,000ths of an inch) to insert origin of DXF
	// drawing in KICAD drawing.
	int xoffset;
	int yoffset;
	// layer to draw on in KICAD drawing (28 is board outline)
	int layer; 
	// thickness in tenths of mils of drawing in Kicad
	int thickness;
public:
	// constructor
	Dxf2BrdFilter(int xoffset, int yoffset, int layer, int thickness);
	
};


// lines are the easiest to convert: both kicad and dxf use the same
// format: pairs of points for the start and end.
void Dxf2BrdFilter::addLine(const DL_LineData& d) {
	

	int x1 = 0;
	int x2 = 0;
	int y1 = 0;
	int y2 = 0;

	convert(d.x1, d.y1, x1, y1);
	convert(d.x2, d.y2, x2, y2);

	std::cout << "$DRAWSEGMENT" << std::endl;	
	std::cout << "Po 0 " << x1 << " " << y1 << " " 
		  << x2 << " " << y2 << " " << thickness << std::endl;
	std::cout << "De " << layer << " 0 900 0 0" << std::endl;
	std::cout << "$endDRAWSEGMENT" << std::endl;
    
}
// Circles are a bit more complex. Kicad uses two points: one at the
// center, and one on the circumference to describe a circle, while
// DXF uses a center and radius. 

// not only that, but we need to generate drill holes in the PCB
// instead of just drawing circles on the PCB in order to do this, we
// add a module with a single pin, that is specified as being
// mechanical: that is, it is not plated, and not connected to any
// net.
void Dxf2BrdFilter::addCircle(const DL_CircleData& d) {
	
	int cx = 0;
	int cy = 0;
	int xend, yend;
	int rad = (int)d.radius * 10;
	int crad = rad + 150;

	convert(d.cx, d.cy, cx, cy);
	yend = cy;
	xend = cx + rad;
	std::cout << "$MODULE MountingHole" << std::endl;
	std::cout << "Po " << cx << " " << cy << 
		" 0 15 5052AEE8 5052AC20 ~~" << std::endl;
	std::cout << "Li MountingHole" << std::endl;
	std::cout << "Cd Simple Mounting Hole" << std::endl;
	std::cout << "Kw DEV" << std::endl;
	std::cout << "Sc 5052AC20" << std::endl;
	std::cout << "AR 1pin" << std::endl;
	std::cout << "Op 0 0 0" << std::endl;
	std::cout << "T0 0 -1200 400 400 0 100 N I 21 N \"Mounting Hole\"" << std::endl;
	std::cout << "T1 0 1100 400 400 0 100 N I 21 N \"P***\"" << std::endl;

	std::cout << "$PAD" << std::endl;
	std::cout << "Sh \"\" C " << crad << " " 
		  << crad << " 0 0 0" << std::endl;
	std::cout << "Dr " << rad << " 0 0" << std::endl;
	std::cout << "At HOLE N 00E0FFFF" << std::endl;
	std::cout << "Ne 0 \"\"" << std::endl;
	std::cout << "Po 0 0" << std::endl;
	std::cout << "$EndPAD" << std::endl;
	std::cout << "$EndMODULE  MountingHole" << std::endl;


	// draw a picture of the hole for alignment purposes
	std::cout << "$DRAWSEGMENT" << std::endl;
	std::cout << "Po 3 " << cx << " " << cy << " " << xend << " " << yend <<" 150" << std::endl;
	std::cout << "De 24 0 900 0 0" << std::endl;
	std::cout << "$EndDRAWSEGMENT" << std::endl;

	
}

// This is the most complex conversion. Kicad only allows arcs of 90
// degrees, which immediately eliminates many possible DXF
// arcs. Additionally, the format for the arcs is extremely weird:
// Kicad uses two points to describe an arc: the first point is
// located at the center of the arc. The second point is located at
// one terminus of the arc. The other terminus is automatically
// defined by moving -90 degrees from the start point.

// in addition, to simplify the logic: this routine only supports 180
// arcs that are located at 90 degree angles. 
void Dxf2BrdFilter::addArc(const DL_ArcData& d) {

	int xstart = 0;
	int ystart = 0;
	int xend = 0;
	int yend = 0;
	int a1 = d.angle1;
	int a2 = d.angle2;

	int ka1;
	int ka2;
	if(a1 % 90 != 0 || a1 == 0) {
		std::cerr << "Arc not at 90 degrees!" << std::endl;
		return;
	}
	if(a1 - a2 != 180 && a1 - a2 != -180) {
		std::cerr << "Arc not 180 degrees long!" << std::endl;
		return;
	}
		
	
	// in order to draw the semicircles, we split the single DXF
	// arc into two 90 degree kicad arcs. In order to draw this
	// correctly, we need to calculate the end points for these
	// arcs.
	if (a1 == 90 && a2 == 270) {
		ka1 = 180;
		ka2 = 270;
	} else if (a1 == 270 && a2 == 90) {
		ka1 = 360;
		ka2 = 90;
	} else if (a1 == 360 && a2 == 180) {
		ka1 = 90;
		ka2 = 180;
	} else if (a1 == 180 && a2 == 360) {
		ka1 = 270;
		ka2 = 360;
	} else {
		std::cerr << "weird angles: " << a1 << " " << a2 << std::endl;
		return;
	}	
	convert(d.cx, d.cy, xstart, ystart);
		
	convertangle(d.cx, d.cy, d.radius, ka1, xend, yend);
	std::cout << "$DRAWSEGMENT" << std::endl;
	std::cout << "Po 2 " << xstart << " " << ystart << " " 
		  << xend << " " << yend << " " << thickness << std::endl;
	std::cout << "De " << layer << " 0 900 0 0" << std::endl;
	std::cout << "$endDRAWSEGMENT" << std::endl;

	convert(d.cx, d.cy, xstart, ystart);

	convertangle(d.cx, d.cy, d.radius, ka2, xend, yend);
	std::cout << "$DRAWSEGMENT" << std::endl;
	std::cout << "Po 2 " << xstart << " " << ystart << " " 
		  << xend << " " << yend << " " << thickness << std::endl;
	std::cout << "De " << layer << " 0 900 0 0" << std::endl;
	std::cout << "$endDRAWSEGMENT" << std::endl;






}

// constructor
Dxf2BrdFilter::Dxf2BrdFilter(int xoffset =X_OFFSET,  int yoffset =Y_OFFSET, 
			     int layer = LAYER, int thickness = LINE_THICKNESS) : 
	xoffset(xoffset), yoffset(yoffset), layer(layer), thickness(thickness) {}

void Dxf2BrdFilter::convert(double xin, double yin, int &xout, int &yout)
{
	xout = (int)(xin * 10) + xoffset;
	yout = yoffset - (int)(yin * 10);


}

void Dxf2BrdFilter::convertangle(double xin, double yin, 
				 double radius, double angle, int &xout, int &yout)
{
	convert(xin, yin, xout, yout);
	int rad = (int)(radius * 10);
	switch((int)angle) {
	case 360:
		xout += rad;
		break;
	case 90:
		yout -= rad;
		break;
	case 180:
		xout -= rad;
		break;
	case 270:
		yout += rad;
		break;
	default:
		std::cerr << "invalid angle: " << angle << std::endl;
	}
	
}

int main(int argc, char ** argv)
{
	if (argc < 2)
		return -1;
	Dxf2BrdFilter f;
	DL_Dxf* dxf = new DL_Dxf();
	
	if (!dxf->in(argv[1], &f)) {
		std::cerr << "drawing could not be opened.\n";
	}
	delete dxf; 
	return 0;
}

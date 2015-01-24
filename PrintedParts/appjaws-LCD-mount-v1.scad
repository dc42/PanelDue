//appjaws-LCD-mount - parameterised connectors.

include <MCAD/regular_shapes.scad>;
length=36;//U Bracket and/or Flange screw hole centres.
wall=6;
rotatingsphere=10;
radius=rotatingsphere/2;
gap=rotatingsphere+wall*2;//u-bracket gap
sc=1.8;//screw clearence
tolerence=1.0;
$fn=100;

// size is a vector [w, h, d]
module roundedBox(size, radius, sidesonly)
{
	rot = [ [0,0,0], [90,0,90], [90,90,0] ];
	if (sidesonly) {
		cube(size - [2*radius,0,0], true);
		cube(size - [0,2*radius,0], true);
		for (x = [radius-size[0]/2, -radius+size[0]/2],
				 y = [radius-size[1]/2, -radius+size[1]/2]) {
			translate([x,y,0]) cylinder(r=radius, h=size[2], center=true);
		}
	}
	else {
		cube([size[0], size[1]-radius*2, size[2]-radius*2], center=true);
		cube([size[0]-radius*2, size[1], size[2]-radius*2], center=true);
		cube([size[0]-radius*2, size[1]-radius*2, size[2]], center=true);

		for (axis = [0:2]) {
			for (x = [radius-size[axis]/2, -radius+size[axis]/2],
					y = [radius-size[(axis+1)%3]/2, -radius+size[(axis+1)%3]/2]) {
				rotate(rot[axis]) 
					translate([x,y,0]) 
					cylinder(h=size[(axis+2)%3]-2*radius, r=radius, center=true);
			}
		}
		for (x = [radius-size[0]/2, -radius+size[0]/2],
				y = [radius-size[1]/2, -radius+size[1]/2],
				z = [radius-size[2]/2, -radius+size[2]/2]) {
			translate([x,y,z]) sphere(radius);
		}
	}
}
module ubracket(){
difference() {
		roundedBox([gap+wall*2,gap+wall,wall*2], 5, true);
		translate([wall,0,-tolerence/2])roundedBox([gap+wall,gap-wall,wall*2+tolerence*2], 5, true);
		//#translate([0,-wall,-wall/2]) cube([gap+wall,gap+wall,wall]); //U cut out
		translate([gap-wall*2,wall*2.5,0]) rotate([90,0,0]) cylinder(gap+wall*2,sc,sc); //U screw holes
		translate([gap-wall*2,0,0]) sphere(rotatingsphere); //rotating surface
}}

module Flange() {
	difference() {
		union() {
		translate ([0,0,-30]) hull(){
			cylinder(h=10,r=12);
			translate([-2-wall/2,-radius,0]) cube([wall+3,radius*2,length]);  }
			intersection() {
			union() {
			rotate([90,0,0])translate([0,5,-rotatingsphere])cylinder(h=gap+wall+20,r=radius+tolerence);//arm end radius
					}
				translate([0,0,radius]) sphere(rotatingsphere);
				}
			}
			//rotate([90,0,0]) translate([0,radius,-radius-wall]) cylinder(gap,3,3); // mount slot
			rotate([90,0,0]) translate([0,6.7,-rotatingsphere]) cylinder(gap,1.8,1.8); // mount slot
			rotate([90,0,0]) translate([-1.8,3,-rotatingsphere]) cube([3.6,4,gap]); // mount slot
			rotate([90,0,0]) translate([0,3.7,-rotatingsphere]) cylinder(gap,1.8,1.8); // mount slot
		}}//end module flange

				// ubracket();
				translate([18,0,-5]) Flange();translate([length-gap-wall,0,0])ubracket();//flange joined with ubracket
				//Flange();//flange				


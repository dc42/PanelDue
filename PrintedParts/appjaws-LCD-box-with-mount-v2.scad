//This is based on the original by Dave (dmould) - thank you

// Print variables:
// 1) There is an overhang as the case starts printing in order to print the rounded front edges.
//    So long as the MMRadius (MMMRad) is small and the first layer height on the 3D printer is 
//		not too thin the printer should cope.  If however the overhang proves to be too much to
//		print set the "RelieveOverhang" variable non-zero up to 100 and the initial overhang will
//    be reduced proportionately to that number at the expense of a less rounded edge.
//		(A value less than 30 is usually plenty sufficient).  Set to 100 to see the effect.
// 2) Set the "ShowPCB" variable to "true" to show the PCB shapes.
//    This is only in order to see how the PCBs will fit in the case, and should
//    not be used when generating an STL to print
// 3) If "PCBautoplace" is "true", then parameters PCBpos and PCBrot are ignored
//		and the PCB is positioned with its bottom right corner in the bottom right corner
//		of the case, but leaving a 1mm gap.
// 4) If "Front" is set to true then parameters are interpreted as being viewed
//		from the front of the LCD, otherwise they are viewed from the back
Right=true;				//controller board true = right of screen, change to false for left
Bottom=false;				//true if controller board below the display (Itead 4.3 inch display)
Lid=false;				//set to false if lid not required
screw=true;				//set if screw lid
Encoder=false;			//true if hole wanted for rotay encoder
RelieveOverhang=0;		// see note 1 above
extraspace=5;			//main extra space
SmTol=0.1;				//small tolerence
LidSep=10;				// seperation between box and lid
Tol=1;					//main tolerence
ShowPCB=false;
PCBautoplace=true;
Front=true;
MountHeight=32;
// Set up the main enclosure size
MLen=159;			// Cavity length
MWid=76;			// Cavity width
MHt=21;				// Cavity height
MWall=2;			// Main box MWall thickness
MBase=2;			// Main box MBase (front panel) thickness
MRad=MWall;			// MMRadius of box edges

// Set the positions & orientation of LCD and PCB within the enclosure
// NOTE - these are offsets from center of controller PCB or LCD viewing area to center of enclosure
PCBpos=[60,0];		// XY position of controller PCB center from case center.
PCBrot=0;			// Degrees to rotate controller PCB within enclosure (probably not needed)
LCDpos=[-18,0];	// XY position of LCD visible screen center from case center.
LCDrot=0;			// Degrees to rotate LCD within enclosure (probably not needed)

// LCD display module parameters
// NOTE - use any origin position you want and reference everything to that.
//        I have chosen the bottom left of the LCD screen as the origin, but
//        use anything and the code will sort it out.
//        co-ordinates are LCD display side down unless "Front" is "true".
LCDscrn=[0,106,0,67.8];			// The edges of the whole LCD screen area [Left,Right,Bottom,Top]
								// Note I chose bottom left as LCD module origin (0,0)
LCDview=[3,103,7,64.8];		// The edges of the visible region of the LCD screen (L,R,B,T)
LCDmounts=[						// Enter the relative coordinates [X,Y] of each mounting hole
				[-3.5,0],			// You may have as many mounting holes as you like
				[110.5,0],		// Each hole is defined separately for flexibility
				[-3.5,67.8],		// so that holes do not have to be on a rectangular pitch
				[110.5,67.8]		// ... add/remove as needed for modules with more/less mounting holes
			 ];					// ...

LCDpcb =[-7,113,-3,71];	// The edges of the LCD PCB [Left,Right,Bottom,Top]
LCDbez=1;						// This is the thickness covering the non-visible
								// part of the LDC display (MBase thickness - recess)
LCDheight=4.2;					// Height of top of LCD above PCB (mount standoff height)
LCDmnthole=2;					// Hole to take self-tapping screw
LCDbossDia=5;					// Diameter of boss under each mounting hole
LCDbossD2=LCDbossDia*2;		// MBase diameter of boss supports
LCDbossFR=(LCDbossDia-LCDmnthole)/2; // Bottom fillet MMRadius of bosses
LCDsupW=1;						// Width of boss supports

// Controller PCB module parameters
// NOTE - use any origin position you want and reference everything to that.
//        I have chosen the bottom left corner of the PCB as the origin, but
//        use anything and the code will sort it out.
//        co-ordinates are from back of PCB (looking into back of case) unless "Front" is true.
PCB =[0,47.3,0,69];				// The edges of the controller PCB [Left,Right,Bottom,Top]
PCBmounts=[						// Enter the relative coordinates [X,Y] of each mounting hole
			[34,(MWid/2)-12],	// You may have as many mounting holes as you like
			[34,(MWid/2)+12],	// Each hole is defined separately for flexibility
			 ];					// Add as needed
PCBshaftE=[34,MWid/2];			// XY co-ordinates of rorary encoder shaft
PCBshaftDia=7;					// Diameter of hole for rotary encoder shaft
PCBheight=10;					// Height controller PCB above case MBase(mount standoff height)
PCBmnthole=2;					// Hole to take self-tapping screw
PCBbossDia=5;					// Diameter of boss under each mounting hole
PCBbossD2=PCBbossDia*2;		// MBase diameter of boss supports
PCBbossFR=(PCBbossDia-PCBmnthole)/2; // Bottom fillet MMRadius of bosses
PCBsupW=1;						// Width of boss supports
USBpos=[0,-MWid/2+MWall*2,MHt/2];	// Distance of center of USB connector from bottom of PCB
USBsize=[8,3];					// Length and height of USB connector on PCB
USBclear=3;						// Clearance to leave all around USB connector
resetholeDia=2;
resetoffset=[36,20];
eraseoffset=[36,20];
DuetConLen=10;
DuetConWid=6;
Lidmounts=[						// Enter the relative coordinates [X,Y] of each mounting hole
		[(MLen/2)-2,(MWid/2)-2],	// You may have as many mounting holes as you like
		[(MLen/2)-2,-(MWid/2)+2],	// Each hole is defined separately for flexibility
		[-(MLen/2)+2,-(MWid/2)+2],	// so that holes do not have to be on a rectangular pitch
		[-(MLen/2)+2,(MWid/2)-2]	//  add/remove as needed for modules with more/less mounting holes
			 ];						// ...
Lidstandoffheight=MHt;			// Height of top of LCD above PCB (mount standoff height)
Lidmnthole=2;					// Hole to take self-tapping screw
Lidmntclearhole=2.2;			// hole clearence for lid screws
LidbossDia=5;					// Diameter of boss under each mounting hole
LidbossD2=LidbossDia*2;		// MBase diameter of boss supports
LidbossFR=(LidbossDia-Lidmnthole)/2; // Bottom fillet MMRadius of bosses
LidsupW=1;						// Width of boss supports
$fn=50;  // 50 is good for printing, reduce to render faster while experimenting with parameters


/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

// CODE

PCBx=PCBautoplace ? MLen/2-(PCB[1]-PCB[0])/2-1 : PCBpos[0];
PCBy=PCBautoplace ? -MWid/2+(PCB[3]-PCB[2])/2+1 : PCBpos[1]; 

mirror([Front ? 1:0,0,0])
{
	difference()
	{
		union()
		{
			RoundBox(MLen+2*MWall,MWid+2*MWall,MHt+MBase,MRad,MWall,MBase);
			translate([LCDpos[0],LCDpos[1],0]) rotate([0,0,LCDrot]) LCDmnt();
			translate([PCBx,PCBy,0]) rotate([0,0,PCBrot]) PCBmnt();
		}
		translate([LCDpos[0],LCDpos[1],0]) rotate([0,0,LCDrot]) 	LCDcutout();
		translate([USBpos[0],USBpos[1],USBpos[2]]) 	rotate([0,0,PCBrot]) USBcutout();
		if (Encoder) {
			translate([PCBx,PCBy,0]) rotate([0,0,PCBrot]) ShaftCutout();
		}
		ResetCutout();
		EraseCutout();
	}
	if (ShowPCB){
		translate([LCDpos[0],LCDpos[1],0])
		rotate([0,0,LCDrot])
		LCDshape();
	}
	if (ShowPCB){
		translate([PCBx,PCBy,0])
		rotate([0,0,PCBrot])
		PCBshape();
	}
}

module RoundBox(Len,Wid,Ht,Rad,Wall,Base)
{
	translate([0,0,-Rad*(RelieveOverhang/100)])
	difference()
	{
		RoundCube(Len,Wid,Ht+Rad+Rad*(RelieveOverhang/100)+1,Rad);
		translate([-Len,-Wid,Ht+Rad*(RelieveOverhang/100)])
			cube([2*Len,2*Wid,Rad+2]);
		translate([0,0,Base+Rad*(RelieveOverhang/100)])
			RoundCube(Len-2*Wall,Wid-2*Wall,Ht+Rad+1,Rad-Wall);
		translate([-500,-500,-.1])
			cube([1000,1000,Rad*(RelieveOverhang/100)+.1]);
	}
}

module RoundCube(Len,Wid,Ht,Rad)
{
	translate([-Len/2,-Wid/2+Rad,Rad])
	cube([Len,Wid-2*Rad,Ht-2*Rad]);
	translate([-Len/2+Rad,-Wid/2,Rad])
	cube([Len-2*Rad,Wid,Ht-2*Rad]);
	hull()
	{
		translate([-(Len/2-Rad),-(Wid/2-Rad),Rad]) 	sphere(r=Rad);
		translate([-(Len/2-Rad),+(Wid/2-Rad),Rad]) 	sphere(r=Rad);
		translate([+(Len/2-Rad),-(Wid/2-Rad),Rad]) 	sphere(r=Rad);
		translate([+(Len/2-Rad),+(Wid/2-Rad),Rad]) 	sphere(r=Rad);
		translate([-(Len/2-Rad),-(Wid/2-Rad),Ht-Rad]) sphere(r=Rad);
		translate([-(Len/2-Rad),+(Wid/2-Rad),Ht-Rad]) sphere(r=Rad);
		translate([+(Len/2-Rad),-(Wid/2-Rad),Ht-Rad]) sphere(r=Rad);
		translate([+(Len/2-Rad),+(Wid/2-Rad),Ht-Rad]) sphere(r=Rad);
	}
}

module Lid(){
	translate([0,LidSep+MWid,0])RoundBox(MLen+2*MWall,MWid+2*MWall,MBase+2,MRad,MWall,MBase);
	translate([0,LidSep+MWid,4])RoundCube(MLen-Tol,MWid-Tol,MBase+2,0);
}

module Lidscrewhole(){

	for (Mnt=Lidmounts) {
		translate([Mnt[0],Mnt[1],-2]) cylinder(h=Tol*2+MBase*2,r=Lidmntclearhole);
	}

	for (Mnt=Lidmounts) {
		translate([Mnt[0],Mnt[1],MBase+Tol]) cylinder(h=MWall*2,r=5.5);
	}
}

module Lidmnt()  // This creates the mounts for the Lid
{
	// Make the bosses
	for (Mnt=Lidmounts)
	translate([Mnt[0],Mnt[1],MBase])
	rotate([0,0,45])
	Boss(Lidmnthole,LidbossDia,LidbossD2,MHt,LidsupW);
}

// This creates the mounts for the controller PCB
module PCBmnt()
{
translate([-PCB[0]-(PCB[1]-PCB[0])/2,-PCB[2]-(PCB[3]-PCB[2])/2,0])
	{
	// Make the bosses
	for (Mnt=PCBmounts)
	translate([Mnt[0],Mnt[1],MBase])
	Boss(PCBmnthole,PCBbossDia,PCBbossD2,PCBheight,PCBsupW);
	}
}

// This creates the controller PCB shape (as required to help with positioning
module PCBshape()
{
translate([-PCB[0]-(PCB[1]-PCB[0])/2,-PCB[2]-(PCB[3]-PCB[2])/2,0])
	{
	// Make the PCB
		difference()
		{
		translate([PCB[0],PCB[2],PCBheight+MBase])
		cube([PCB[1]-PCB[0],PCB[3]-PCB[2],1.5]);

		for (Mnt=PCBmounts)
		translate([Mnt[0],Mnt[1],MBase])
		Cross(3,100);
		}
	}
}

// This creates the USB cutout
module USBcutout()
{
translate([MLen/2-.1,-PCB[2]-(PCB[3]-PCB[2])/2+USBpos[1]-USBsize[0]/2-USBclear,MBase+PCBheight-USBsize[1]-USBclear])
cube([MWall+.2,USBsize[0]+2*USBclear,USBsize[1]+2*USBclear]);
}

// This creates the shaft cutout
module ShaftCutout()
{
translate([-(PCB[1]-PCB[0])/2+PCBshaftE[0],-(PCB[3]-PCB[2])/2+PCBshaftE[1],-.1])
cylinder(r=PCBshaftDia/2,h=MBase+.2);
}

module ResetCutout()
{
translate([resetoffset[0],resetoffset[1],-1]) cylinder(h=MWall*4,r=resetholeDia/2);
}

module EraseCutout()
{
translate([eraseoffset[0],eraseoffset[1],-1]) cylinder(h=MWall*4,r=resetholeDia/2);
}

// This creates the mounts for the LCD
module LCDmnt()
{
translate([-LCDview[0]-(LCDview[1]-LCDview[0])/2,-LCDpcb[2]-(LCDpcb[3]-LCDpcb[2])/2,0])
	{
	// Make the bosses
	for (Mnt=LCDmounts)
	translate([Mnt[0],Mnt[1],MBase])
	rotate([0,0,45])
	Boss(LCDmnthole,LCDbossDia,LCDbossD2,LCDheight-MBase+LCDbez,LCDsupW);
	}
}

// This creates the LCD PCB and screen shapes (as required to help with positioning)
module LCDshape()
{
translate([-LCDview[0]-(LCDview[1]-LCDview[0])/2,-LCDview[2]-(LCDview[3]-LCDview[2])/2,0])
	{
		difference()
		{
			union()
			{
			translate([LCDpcb[0],LCDpcb[2],LCDbez+LCDheight])
			cube([LCDpcb[1]-LCDpcb[0],LCDpcb[3]-LCDpcb[2],1.5]);
			translate([LCDscrn[0],LCDscrn[2],LCDbez])
			cube([LCDscrn[1]-LCDscrn[0],LCDscrn[3]-LCDscrn[2],LCDheight]);
			}
		for (Mnt=LCDmounts)
		translate([Mnt[0],Mnt[1],0])
		Cross(3,100);
		}
	}
}
// This creates the case cutout for the LCD display
module LCDcutout()
{
translate([-LCDview[0]-(LCDview[1]-LCDview[0])/2,-LCDview[2]-(LCDview[3]-LCDview[2])/2,0])
	{
	translate([LCDscrn[0],LCDscrn[2],LCDbez])
	cube([LCDscrn[1]-LCDscrn[0],LCDscrn[3]-LCDscrn[2],100]);
	translate([LCDview[0],LCDview[2],-.1])
	cube([LCDview[1]-LCDview[0],LCDview[3]-LCDview[2],100]);
	}
}

module Boss(HoleD,Dia1,Dia2,Ht,MWall)
{
	difference()
	{
		union()
		{
		cylinder (r=Dia1/2,h=Ht);
		Xsupport(Dia1,Dia2,Ht,MWall);
		CirFillet(Dia1,MWall);
		}
	translate([0,0,-.1])
	cylinder(r=HoleD/2,h=Ht+.2);
	}
}

module Xsupport(Dia1,Dia2,Ht,MWall)
{
	difference()
	{
		union()
		{
		translate([-Dia2/2,-MWall/2,0])
		cube([Dia2,MWall,Ht]);
		translate([-MWall/2,-Dia2/2,0])
		cube([MWall,Dia2,Ht]);
		}
		difference()
		{
		translate([0,0,-1])
		cylinder(r=Dia2,h=Ht+2);
		cylinder(r1=Dia2/2,r2=Dia1/2,h=Ht+.1);
		}
	}
}

module CirFillet(D,R)
{
	difference()
	{
	rotate_extrude()
	translate([D/2,0,0])
	square(R);

	rotate_extrude()
	translate([D/2+R,R,0])
	circle(r=R);
	}
}

// Draws a negative crosshair		
module Cross(R,H)
{
	rotate([0,0,0])
	Quad(R,H);
	rotate([0,0,90])
	Quad(R,H);
	rotate([0,0,180])
	Quad(R,H);
	rotate([0,0,270])
	Quad(R,H);
}

module Quad(R,H)
{
	difference()
	{
		cylinder(r=R,h=H);

		translate([-500,-500,-.1])
		cube([500.1,1000,H+.2]);
		translate([-500,-500,-.1])
		cube([1000,500.1,H+.2]);
	}
}

module Box(){
	// RoundBox(MLen+2*MWall,MWid+2*MWall,MHt+MBase,MRad,MWall,MBase);
	 if(Lid){  
		if (screw){ 
			Lidmnt();
			if (Right){
				difference(){ 
					Lid();
					translate([0,10+MWid,1])Lidscrewhole();
				}
			}
			else {
				translate ([0,(-LidSep-MWid)*2,0])
					difference(){ 
						Lid();
						translate([0,10+MWid,1])Lidscrewhole();
					}
			}
				//else {Lid();}
		}
	}
}

Box();
if(Right){ 
	translate([0,-MountHeight-MWid/2,(MHt/2)+Tol*2])
		rotate([90,90,0])
			import("appjaws-LCD-box-mount.stl");
}
else {
	translate([0,MountHeight+MWid/2,(MHt/2)+Tol*2])
		rotate([90,90,180])
			import("appjaws-LCD-box-mount.stl");
}

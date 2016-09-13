// Briley Cantwell 9/12/2016
// Projectile is a 2D physics game where the player must choose the launch speed and angle of a ball to hit a target.
// The executable (.exe) is available in the "Release" folder

#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/ImageIo.h"
#include "cinder/gl/Texture.h"
#include "cinder/Text.h"
#include "cinder/Utilities.h"
#include "cinder/ImageIo.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Fbo.h"

#include "cinder/TriMesh.h"
#include "cinder/Camera.h"
#include "Resources.h"
#include <math.h>

using namespace ci;
using namespace ci::app;
using namespace std;

// return random integer, inclusive of low and high 
int randomInt(int low, int high)
{
    return low + (rand() % (high - low + 1));
}

// int to string
string toString(int in)
{
	return to_string(static_cast<long double>(in));
}

// float to string
string toString(float in)
{
	return to_string(static_cast<long double>(in));
}

// string to int
int toInt(string str)
{
	return atoi(str.c_str());
}

// string to float
float toFloat(string str)
{
	return atof(str.c_str());
}

// Plat, short for "Platform", represents a rectangle that the ball can bounce off of:
// This includes screen borders, ground, and floating platforms.
struct Plat
{
	Plat( Rectf r, int tn ): rect(r), texnum(tn) {}

	Rectf rect; // screen coordinates of the Plat
	int texnum; // texture number, 0-grass 1-sand 2-rock
};

// Sweeper quickly simulates a possible launch angle and indicates if this angle will result in a hit target.
struct Sweeper
{
	// constructor:
	Sweeper( vector< Plat* > * v, Rectf ballr, float ang, float spe, int targx, int targy, float xw, float yw ):
		vp(v), ballrect(ballr), angle(ang), speed(spe), targetx(targx), targety(targy), xwind(xw), ywind(yw),
			finished(false), successful(false), closest(-999.9f)
		{
			balldrawrect = ballrect;

			for( int k = 0; k < 10; k++ )
				ballshadow[k] = Rectf( -20-k, 0, -10-k, 10 );

			xvel = spe*cos(0.0174533f*ang)/6.0f;
			yvel = -spe*sin(0.0174533f*ang)/6.0f;
		}

	vector< Plat* > * vp; // pointer to vector of platform pointers that make up the current screen
	Rectf ballrect; // screen position of the ball (in floats)
	Rectf balldrawrect; // screen position of the ball (rounded to nearest int)
	Rectf ballshadow[10]; // black shadow animated behind the ball. Makes movement easier to follow.
	float xvel; // horizontal velocity, in pixels per frame
	float yvel; // vertical velocity, in pixels per frame
	float angle; // launch angle in degrees
	float speed; // speed in pixels per frame
	int targetx, targety; // screen coordinates of the center of the target
	float xwind, ywind; // the horizontal and vertical acceleration that the wind causes on the ball
	
	bool finished; // true if done with simulation, false if simulation still in progress
	bool successful; // true if this launch angle will result in a hit target, false otherwise
	float closest; // the closest distance the ball has been to the target for this simulation

	void simulate(); // loops this simulation to completion
	bool sweepercollision( Rectf r1, Rectf r2 ); // collision function called within simulate()
	int sweepercollblock( Rectf r ); // collision with block function called within simulate()
};


class ProjectileApp : public AppNative {
  public:
	void prepareSettings( Settings* settings );
	void setup();
	void mouseDown( MouseEvent event );
	void mouseUp( MouseEvent event );
	void mouseMove( MouseEvent event );
	void mouseDrag( MouseEvent event );
	void keyDown( KeyEvent event );
	void keyUp( KeyEvent event );
	void update();
	void draw();


	void loadSprites(); // loads in textures once on start

	void generateCourse(); // initializes everything with new platforms and wind
	bool collision( Rectf r1, Rectf r2 ); // collision function called within update()
	int collblock( Rectf r ); // collision with block function called within update()

	void updateWindFbo(); // draws the texture of wind speed and direction
	void updateMenuFbo(); // draws the texture of the button menu
	void makeHelpFbo(); // draws the texture of the help pop-up

	void reset(); // resets the ball and launch angle within the current platform layout

	Rectf origballrect; // original screen coordinates of the ball within the current platform layout
	Rectf ballrect; // screen position of the ball (in floats)
	Rectf balldrawrect; // screen position of the ball (rounded to nearest int)
	Rectf ballshadow[10]; // black shadow animated behind the ball. Makes movement easier to follow.

	float xvel; // horizontal velocity, in pixels per frame
	float yvel; // vertical velocity, in pixels per frame
	bool aiming; // true if the user is currently able to change the launch angle and speed, false otherwise
	float angle; // launch angle in degrees
	float speed; // speed in pixels per frame
	int targetx, targety; // screen coordinates of the center of the target
	int hit; // 0 if haven't hit target yet, increments up from 1 to 30 after target hit.
	int strokes; // number of launches so far before hitting the target (similar to golf strokes)
	float windstrength; // the accelerational effect that the wind has on the ball
	float windangle; // the direction the wind pushes the ball
	float xwind, ywind; // the horizontal and vertical acceleration that the wind causes on the ball

	vector< Plat* > plats; // vector of platform pointers that make up the current screen

	int searching; // 0 by default, 1 if searching for a successful angle with sweepers, 4 if successfully found a winning angle, 5 if no winning angle exists
	string searchingstring; // displays sweeping progress to user
	Sweeper* sweeper; // pointer to active sweeper
	Sweeper* storedsweeper; // pointer to past sweeper that achieved the ball's closest distance to the target
	float tAngle; // current angle being simulated with a sweeper
	float tSpeed; // current speed being simulated with a sweeper

	bool help; // true if the help pop-up is showing, false otherwise

	int screenx, screeny; // screen width and height in pixels (does not include menu)
	int mousex, mousey; // screen coordinates of the mouse
	int realscreeny; // total screen height in pixels (including the menu)

	bool keystates[30]; // stores true if key is down, false otherwise
	bool keypresses[30]; // stores true if key has just been hit, false otherwise
	bool mousestates[2]; // stores true is mouse click (0-left 1-right) is down, false otherwise
	bool mousepresses[2]; // stores true is mouse click (0-left 1-riht) has just been hit, false otherwise

	Font fo; // Lucida Console font
	Color black; // black

	gl::Texture tex[10]; // textures

	// fbo = Frame Buffer Object
	gl::Fbo windfbo; // drawable texture to display wind speed and direction
	gl::Fbo menufbo; // drawable texture of the button menu
	gl::Fbo helpfbo; // drawable texture of the help pop-up
};

void ProjectileApp::prepareSettings( Settings* settings )
{
	settings->setFullScreen(); // full screen
	settings->setFrameRate(60.0f); // capped at 60 frames per second
}

void ProjectileApp::setup()
{
	srand(static_cast<unsigned int>(time(0))); // random seed set to start time
	
	loadSprites();

	screenx = this->getWindowWidth(); // screen width in pixels
	screeny = this->getWindowHeight()-100; // screen height in pixels (does not include menu)
	realscreeny = screeny + 100; // screen height in pixels (includes menu)
	
	fo = Font("Lucida Console", 28); // initialize lucida console font
	black = Color( 0, 0, 0 ); // initialize black color

	for( int i = 0; i < 30; i++ )
	{	keystates[i] = 0; keypresses[i] = 0; } // all keys are initially up (not pressed)

	sweeper = NULL;
	storedsweeper = NULL;
	searching = 0; // not searching for a winning angle at start

	// Fbo = Frame Buffer Object. Initialize dynamically drawable textures:
	gl::Fbo::Format fmt;
	fmt.setMagFilter( GL_NEAREST );
	windfbo = gl::Fbo( 128, 128, fmt );
	menufbo = gl::Fbo( screenx, 128, fmt );
	helpfbo = gl::Fbo( 512, 512, fmt );
	makeHelpFbo();

	help = true; // help pop-up is showing at start

	generateCourse(); // create new arrangement of platforms and initialize gameplay
}

void ProjectileApp::mouseDown( MouseEvent event )
{
	// mouseDown is called once when mouse (left or right) is clicked
	// mousestates means mouse click (0-left 1-right) is down
	// mousepresses means mouse click (0-left 1-right) has just been hit
	mousex = event.getX();
	mousey = event.getY();
	
	if ( event.isLeft() )
	{
		if ( !mousestates[0] )
			mousepresses[0] = true;
		mousestates[0] = true;
	}
	else if ( event.isRight() )
	{
		if ( !mousestates[1] )
			mousepresses[1] = true;
		mousestates[1] = true;
	}
}

void ProjectileApp::mouseUp( MouseEvent event )
{
	// mouseUp is called once when the mouse (left or right) is released

	mousex = event.getX();
	mousey = event.getY();

	if ( event.isLeft() )
		mousestates[0] = false;
	else if ( event.isRight() )
		mousestates[1] = false;
}

void ProjectileApp::mouseMove( MouseEvent event )
{
	// mouseMove is called every time the mouse moves (while not pressed)
	mousex = event.getX();
	mousey = event.getY();
}

void ProjectileApp::mouseDrag( MouseEvent event )
{
	// mouseDrag is called every time the mouse moves while pressed
	mousex = event.getX();
	mousey = event.getY();
}

void ProjectileApp::keyDown( KeyEvent event )
{
	// keystates stores true is key is down, false otherwise
	// keypresses stores true is key has just been hit, false otherwise

	if ( event.getCode() == event.KEY_LEFT )
	{
		if ( keystates[0] == false )
			keypresses[0] = true;
		keystates[0] = true;
	}
	if ( event.getCode() == event.KEY_RIGHT )
	{
		if ( keystates[2] == false )
			keypresses[2] = true;
		keystates[2] = true;
	}
	if ( event.getCode() == event.KEY_DOWN )
	{
		if ( keystates[3] == false )
			keypresses[3] = true;
		keystates[3] = true;
	}
	if ( event.getCode() == event.KEY_UP )
	{
		if ( keystates[1] == false )
			keypresses[1] = true;
		keystates[1] = true;
	}
	if ( event.getCode() == event.KEY_SPACE )
	{
		if ( keystates[4] == false )
			keypresses[4] = true;
		keystates[4] = true;
	}
	if ( event.getCode() == event.KEY_a )
	{
		if ( keystates[5] == false )
			keypresses[5] = true;
		keystates[5] = true;
	}
	if ( event.getCode() == event.KEY_c )
	{
		if ( keystates[6] == false )
			keypresses[6] = true;
		keystates[6] = true;
	}
	if ( event.getCode() == event.KEY_PERIOD )
	{
		if ( keystates[7] == false )
			keypresses[7] = true;
		keystates[7] = true;
	}
	if ( event.getCode() == event.KEY_COMMA )
	{
		if ( keystates[8] == false )
			keypresses[8] = true;
		keystates[8] = true;
	}
	if ( event.getCode() == event.KEY_m )
	{
		if ( keystates[9] == false )
			keypresses[9] = true;
		keystates[9] = true;
	}
	if ( event.getCode() == event.KEY_r )
	{
		if ( keystates[10] == false )
			keypresses[10] = true;
		keystates[10] = true;
	}
	if ( event.getCode() == event.KEY_TAB )
	{
		if ( keystates[11] == false )
			keypresses[11] = true;
		keystates[11] = true;
	}
	if ( event.getCode() == event.KEY_b )
	{
		if ( keystates[12] == false )
			keypresses[12] = true;
		keystates[12] = true;
	}
	if ( event.getCode() == event.KEY_g )
	{
		if ( keystates[13] == false )
			keypresses[13] = true;
		keystates[13] = true;
	}
	if ( event.getCode() == event.KEY_n )
	{
		if ( keystates[14] == false )
			keypresses[14] = true;
		keystates[14] = true;
	}
	if ( event.getCode() == event.KEY_s )
	{
		if ( keystates[15] == false )
			keypresses[15] = true;
		keystates[15] = true;
	}
	if ( event.getCode() == event.KEY_h )
	{
		if ( keystates[16] == false )
			keypresses[16] = true;
		keystates[16] = true;
	}

	if ( event.getCode() == event.KEY_q )
	{
		if ( keystates[29] == false )
			keypresses[29] = true;
		keystates[29] = true;
	}
	if ( event.getCode() == event.KEY_ESCAPE )
		quit();
}

void ProjectileApp::keyUp( KeyEvent event )
{
	// keystates stores true is key is down, false otherwise
	// keypresses stores true is key has just been hit, false otherwise

	if ( event.getCode() == event.KEY_LEFT )
		keystates[0] = false;
	if ( event.getCode() == event.KEY_RIGHT )
		keystates[2] = false;
	if ( event.getCode() == event.KEY_DOWN )
		keystates[3] = false;
	if ( event.getCode() == event.KEY_UP )
		keystates[1] = false;
	if ( event.getCode() == event.KEY_SPACE )
		keystates[4] = false;
	if ( event.getCode() == event.KEY_a )
		keystates[5] = false;
	if ( event.getCode() == event.KEY_c )
		keystates[6] = false;
	if ( event.getCode() == event.KEY_PERIOD )
		keystates[7] = false;
	if ( event.getCode() == event.KEY_COMMA )
		keystates[8] = false;
	if ( event.getCode() == event.KEY_m )
		keystates[9] = false;
	if ( event.getCode() == event.KEY_r )
		keystates[10] = false;
	if ( event.getCode() == event.KEY_TAB )
		keystates[11] = false;
	if ( event.getCode() == event.KEY_b )
		keystates[12] = false;
	if ( event.getCode() == event.KEY_g )
		keystates[13] = false;
	if ( event.getCode() == event.KEY_n )
		keystates[14] = false;
	if ( event.getCode() == event.KEY_s )
		keystates[15] = false;
	if ( event.getCode() == event.KEY_h )
		keystates[16] = false;

	if ( event.getCode() == event.KEY_q )
		keystates[29] = false;
}

void ProjectileApp::update()
{
	if ( help ) // if help pop-up is showing
	{
		if ( keypresses[4] || (mousepresses[0] && mousey > screeny/2+144 && mousey < screeny/2+194 && mousex > screenx/2-100 && mousex < screenx/2+100) )
		{
			// clicked okay or hit space to close the help pop-up
			help = false;
			updateMenuFbo();
		}

		for( int i = 0; i < 30; i++ )
			keypresses[i] = false;
		for( int i = 0; i < 2; i++ )
			mousepresses[i] = false;
		return;
	}

	if ( mousepresses[0] ) // left mouse is down
	{
		if ( searching != 1 && mousey > screeny+20 && mousey < screeny+80 ) // not searching for winning launch angle but clicked inside menu
		{
			if ( mousex > screenx/4-screenx/12 && mousex < screenx/4+screenx/12 ) // clicked reset button
				reset();
			else if ( mousex > screenx/2-screenx/12 && mousex < screenx/2+screenx/12 ) // clicked new course button
				generateCourse();
			else if ( mousex > screenx*3/4-screenx/12 && mousex < screenx*3/4+screenx/12 ) // clicked "Run Solver Algorithm" button
			{
				if ( searching == 0 )
				{
					// begin sweep for hole-in-one launch
					reset();
					searching = 1;
					tAngle = 80.0f;
					tSpeed = 20.0f;
					sweeper = new Sweeper( &plats, ballrect, tAngle, tSpeed, targetx, targety, xwind, ywind );
					searchingstring = "0% Exhausted";
					updateMenuFbo();
					disableFrameRate(); // turn off framerate to maximize solving speed
				}
			}
			else if ( mousex < screenx/12) // clicked help button
			{
				help = true;
				updateMenuFbo();
			}
			else if ( mousex > screenx-screenx/12 ) // clicked quit button
				quit();
		}
		if (mousey > screeny+10 && mousey < screeny+100 && mousex > screenx*3/4-screenx/11 && mousex < screenx*3/4+screenx/11) // clicked to show one-shot solution
		{
			if ( searching == 4 || searching == 5 )
			{
				reset();
				angle = tAngle;
				speed = tSpeed;
				aiming = true;
				keypresses[4] = true;
			}
		}
	}

	// keypresses
	if ( searching != 1 )
	{
		if ( keypresses[10] ) // r - reset
			reset();
		else if ( keypresses[14] ) // n - new course
			generateCourse();
		else if ( keypresses[16] ) // h - help
		{
			help = true;
			updateMenuFbo();	
		}
		else if ( keypresses[15] ) // s - run solver algorithm or show solved one-shot solution
		{
			if ( searching == 0 )
			{
				// begin sweep for hole-in-one
				reset();
				searching = 1;
				//tAngle = 2.0f;
				tAngle = 80.0f;
				tSpeed = 20.0f;
				sweeper = new Sweeper( &plats, ballrect, tAngle, tSpeed, targetx, targety, xwind, ywind );
				searchingstring = "0% Exhausted";
				updateMenuFbo();
				disableFrameRate();
			}
			else if ( searching == 4 || searching == 5 ) // show one-shot solution
			{
				reset();
				angle = tAngle;
				speed = tSpeed;
				aiming = true;
				keypresses[4] = true;
			}
		}
	}


	if ( searching == 1 ) // actively using sweepers to find a winning launch angle and speed (one-shot solution)
	{
		for( int countsol = 0; countsol < 5; countsol++ ) // run 5 times for speed (arbitrary number)
		{
			if ( sweeper->finished ) // the simulation of this launch angle and speed have completed
			{
				if ( sweeper->successful ) // this launch angle and speed result in a hit target
				{
					angle = sweeper->angle;
					speed = sweeper->speed;
					tAngle = angle;
					tSpeed = speed;
					delete sweeper; sweeper = NULL;
					delete storedsweeper; storedsweeper = NULL;
					searching = 4; // success
					updateMenuFbo();
					setFrameRate(60); // restore framerate (it was previously disabled)
					break;
				}
				else // not successful: this launch angle and speed did not result in a hit target
				{
					// if this current sweeper (launch angle and speed) got the ball the closest to the target, store it in storedsweeper for tracking
					if ( storedsweeper == NULL )
						storedsweeper = sweeper;
					else if ( sweeper->closest < storedsweeper->closest )
					{
						delete storedsweeper;
						storedsweeper = sweeper;
					}
					else
					{
						delete sweeper; sweeper = NULL;
					}
				
					tSpeed += 5.0f; // sweep through launch angles and speeds until we find a one-shot solution
					if ( tSpeed > 91.0f )
					{
						tSpeed = 20.0f;
						if ( tAngle < 81.0f )
						{

							tAngle -= 2.0f;
							if ( tAngle < 4.0f )
							{
								tAngle = 90.0f;
							}
						}
						else // second half of sweep angles
						{
							tAngle += 2.0f;
							if ( tAngle > 170.0f ) // finished all sweeps with no successful shot found.
							{
								angle = storedsweeper->angle;
								speed = storedsweeper->speed;
								tAngle = angle;
								tSpeed = speed;
								delete sweeper; sweeper = NULL;
								delete storedsweeper; storedsweeper = NULL;
								searching = 5; // no one-shot solution found
								updateMenuFbo();
								setFrameRate(60); // restore framerate (it was previously disabled)
								break;
							}
						}
					}
					if ( searching != 5 ) // haven't exhausted all launch angles and speeds, still searching
					{
						sweeper = new Sweeper( &plats, ballrect, tAngle, tSpeed, targetx, targety, xwind, ywind );
					}
				}
			}
			else // the current sweeper is not finished, continue simulating
			{
				sweeper->simulate();
			}
		}
	}
	else if ( aiming ) // the user is pressing arrow keys to adjust launch angle and speed
	{
		if ( keystates[0] && angle < 179.0f )
			angle += 0.5f;
		if ( keystates[2] && angle > 1.0f )
			angle -= 0.5f;
		if ( keystates[1] && speed < 90.0f )
			speed += 0.5f;
		if ( keystates[3] && speed > 10.0f )
			speed -= 0.5f;

		if ( keypresses[4] ) // space: launch the ball
		{
			xvel = speed*cos(0.0174533f*angle)/6.0f;
			yvel = -speed*sin(0.0174533f*angle)/6.0f;
			aiming = false;
			strokes++;
		}
	}
	else if ( hit == 0 ) // ball flight in motion, have not hit target yet
	{
		int tx = collblock( ballrect + Vec2f( xvel, yvel ) ); // determine if there is a collision with a platform in the ball's immediate path
		if ( tx != -1 ) // there is a collision
		{
			int collx = collblock( ballrect + Vec2f( xvel, 0 ) ); // is the collision in the x (horizontal) direction?
			int colly = collblock( ballrect + Vec2f( 0, yvel ) ); // is the collision in the y (vertical) direction?

			if ( (collx != -1 && colly != -1) || (collx == -1 && colly == -1) ) // collision is in both x and y directions (it's a corner bounce)
			{
				// the ball moves to contact with the platform, then reverses velocities in the x and y axes.
				float i;
				for( i = 0.05f; i < 1.0f; i += 0.05f )
				{
					if ( collblock( ballrect + Vec2f( xvel*i, yvel*i ) ) != -1 )
						break;
				}
				ballrect += Vec2f( xvel*(i-0.05f), yvel*(i-0.05f) );
				
				if ( tx == 0 ) // grass: medium bounce
				{
					xvel *= -0.5f;
					yvel *= -0.5f;
				}
				else if ( tx == 1 ) // sand: low/weak bounce
				{
					xvel *= -0.25f;
					yvel *= -0.25f;
				}
				else if ( tx == 2 ) // rock: high bounce
				{
					xvel *= -0.9f;
					yvel *= -0.9f;
				}
			}
			else if ( collx != -1 ) // collision is only in the x direction (horizontal bounce)
			{
				// the ball moves to contact with the platform, then reverses velocity in the x axis only.
				float i;
				for( i = 0.05f; i < 1.0f; i += 0.05f )
				{
					if ( collblock( ballrect + Vec2f( xvel*i, yvel*i ) ) != -1 )
						break;
				}
				ballrect += Vec2f( xvel*(i-0.05f), yvel*(i-0.05f) );

				if ( tx == 0 ) // grass: medium bounce
				{
					xvel *= -0.5f;
					//yvel *= 0.5f;
				}
				else if ( tx == 1 ) // sand: low/weak bounce
				{
					xvel *= -0.25f;
					//yvel *= 0.25f;
				}
				else if ( tx == 2 ) // rock: high bounce
				{
					xvel *= -0.9f;
					//yvel *= 0.9f;
				}
			}
			else // if ( colly ) // collision is only in the y direction (vertical bounce)
			{
				// the ball moves to contact with the platform, then reverses velocity in the y axis only.
				float i;
				for( i = 0.05f; i < 1.0f; i += 0.05f )
				{
					if ( collblock( ballrect + Vec2f( xvel*i, yvel*i ) ) != -1 )
						break;
				}
				ballrect += Vec2f( xvel*(i-0.05f), yvel*(i-0.05f) );

				if ( tx == 0 ) // grass: medium bounce
				{
					//xvel *= 0.5f;
					yvel *= -0.5f;
				}
				else if ( tx == 1 ) // sand: low/weak bounce
				{
					//xvel *= 0.25f;
					yvel *= -0.25f;
				}
				else if ( tx == 2 ) // rock: high bounce
				{
					//xvel *= 0.9f;
					yvel *= -0.9f;
				}
			}
		}
		else // there is no collision in the ball's immediate path
			ballrect += Vec2f( xvel, yvel ); // update the ball's position based on its velocities in the x and y direction

		// apply gravity effects
		yvel += 0.1f; // slightly increase the ball's y velocity downward every frame to simulate gravity
		// wind
		xvel += xwind; // slightly change the ball's velocities in the x and y direction to simulate wind
		yvel += ywind;

		for( int i = 0; i < 9; i++ ) // update the ball's shadow for easier motion viewing
			ballshadow[i] = ballshadow[i+1];
		ballshadow[9] = balldrawrect;

		// round floats to nearest int for clear pixel drawing
		balldrawrect = Rectf( (int) (ballrect.x1+0.5f), (int) (ballrect.y1+0.5f), (int) (ballrect.x2+0.5f), (int) (ballrect.y2+0.5f) );
		
		for( int i = 0; i < 9; i++ ) // if the ball has not moved in the last 10 frames, stop updating its position and return controls to the user to aim the ball again
		{
			if ( ballshadow[i].x1 != ballshadow[i+1].x1 ||
				ballshadow[i].y1 != ballshadow[i+1].y1 )
				break;
			if ( i == 8 )
			{
				for( int k = 0; k < 10; k++ )
					ballshadow[k] = Rectf( -20-k, 0, -10-k, 10 );
				aiming = true;
				break;
			}
		}

		// check if the ball has hit the target
		float hyp = sqrt(((ballrect.x1+5.0f)-targetx)*((ballrect.x1+5.0f)-targetx)
			+ ((ballrect.y1+5.0f)-targety)*((ballrect.y1+5.0f)-targety));
		if ( hyp <= 26.0f )
		{
			hit = 1; // successful hit
		}

	}
	else // the ball has already hit the target, update winning pop-up box
	{
		if ( hit < 30 ) // grow the winning pop-up box
			hit++;

		if ( keypresses[4] ) // pressing SPACE creates a new arrangement of platforms
			generateCourse();
	}

	for( int i = 0; i < 30; i++ ) // key presses have already been registered, set them to false so they don't keep firing
		keypresses[i] = false;
	for( int i = 0; i < 2; i++ )
		mousepresses[i] = false;
}

void ProjectileApp::draw()
{
	gl::clear( Color( 0.6f, 0.8f, 1.0f ) ); // set the background color
	
	gl::color(1,1,1); // draw texture colors

	for( int i = 0; i < plats.size(); i++ ) // draw all platforms
		gl::draw( tex[ plats[i]->texnum ], Area(0,0,plats[i]->rect.getWidth(),plats[i]->rect.getHeight()), plats[i]->rect );
	
	gl::enableAlphaBlending(); // make sprite backgrounds appear transparent and not solid white
	gl::draw( tex[3], Area(10,0,56,46), Rectf( targetx-23, targety-23, targetx+23, targety+23 ) ); // draw the target
	gl::color(0,0,0);
	for( int i = 0; i < 10; i++ ) // draw the ball's trailing motion in black
		gl::draw( tex[3], Area(0,0,10,10), ballshadow[i] );
	gl::color(1,1,1);
	gl::draw( tex[3], Area(0,0,10,10), balldrawrect ); // draw the ball

	if ( aiming ) // the user is deciding the angle and speed of the ball's flight before launch
	{
		for( int i = 1; i < 5; i++ )
		{
			gl::draw( tex[3], Area(56,0,76,20), Rectf( ballrect.x1-5, ballrect.y1-5, ballrect.x2+5, ballrect.y2+5 )
				+ Vec2f( speed*cos(0.0174533f*angle)/4.0f*i, -speed*sin(0.0174533f*angle)/4.0f*i ) ); // draw the angle and speed indicator symbols
		}
		
		if ( !(searching >= 1 && searching <= 3) )
			gl::drawString( "Speed: " + toString( speed ) + "\nAngle: " + toString( angle ),
				Vec2f( balldrawrect.x1-20, balldrawrect.y1-120), black, fo ); // draw the angle and speed text
	}
	gl::disableAlphaBlending();

	if ( hit > 0 ) // the ball has hit the target. Draw success pop-up window
	{
		gl::color(0.0f,0.75f,1.0f);
		gl::drawSolidRect( Rectf( screenx/2-(hit*150/30), screeny/2-(hit*80/30), screenx/2+(hit*150/30), screeny/2+(hit*80/30) ) );
		gl::color(0,0,0);
		gl::drawStrokedRect( Rectf( screenx/2-(hit*150/30), screeny/2-(hit*80/30), screenx/2+(hit*150/30), screeny/2+(hit*80/30) ) );
		if ( hit == 30 ) // pop-up has fully formed
		{
			gl::enableAlphaBlending();
			gl::drawStringCentered( "Success!", Vec2f( screenx/2, screeny/2-20 ), black, fo );
			gl::drawStringCentered( "Tries: " + toString( strokes ), Vec2f( screenx/2, screeny/2+5 ), black, fo );
			gl::disableAlphaBlending();
		}
	}

	if ( searching == 1 ) // running algorithm to find a one-shot solution, show progress
	{
		gl::color(0.0f,0.75f,1.0f);
		gl::drawSolidRect( Rectf( screenx/2-250, screeny/2-80, screenx/2+250, screeny/2+80 ) );
		gl::color(0,0,0);
		gl::drawStrokedRect( Rectf( screenx/2-250, screeny/2-80, screenx/2+250, screeny/2+80 ) );
		gl::enableAlphaBlending();
		gl::drawStringCentered( "Brute-Force Sweeper In Progress", Vec2f( screenx/2, screeny/2-20 ), black, fo );
		if ( tAngle < 81.0f )
			gl::drawStringCentered( toString(50-(int)(((tAngle-4.0f)/76.0f)*50.0f))+"% Exhausted", Vec2f( screenx/2, screeny/2+5 ), black, fo );
		else
			gl::drawStringCentered( toString(50 + (int)(((tAngle-90.0f)/80.0f)*50.0f))+"% Exhausted", Vec2f( screenx/2, screeny/2+5 ), black, fo );
		gl::disableAlphaBlending();
	}

	if ( help ) // help pop-up is showing
	{
		gl::color(1,1,1);
		gl::draw( helpfbo.getTexture(), Area(0,0,512,512), Rectf(screenx/2-256, screeny/2-256, screenx/2+256, screeny/2+256) );
		gl::color(0,0,0);
		gl::drawStrokedRect( Rectf(screenx/2-256, screeny/2-256, screenx/2+256, screeny/2+256) );
	}

	gl::color(1,1,1);
	gl::draw( windfbo.getTexture(), Area(0,0,128,128), Rectf(0,0,128,128) ); // draw the arrow and text indicating wind speed and direction
	
	gl::draw( menufbo.getTexture(), Area(0,0,screenx,100), Rectf(0,screeny,screenx,realscreeny) ); // draw the bottom menu bar
	
	if ( searching != 1 && !help && mousey > screeny+20 && mousey < screeny+80 ) // if the mouse is hovering over a menu button, highlight the button
	{
		for( int xx = screenx/4; xx < screenx*3/4-1; xx += screenx/4 )
		if ( mousex > xx - screenx/12 && mousex < xx + screenx/12 )
		{
			gl::enableAlphaBlending();
			gl::color(0.4f, 0.4f, 1.0f, 0.5f);
			gl::drawSolidRect(Rectf(xx-screenx/12, screeny+20, xx+screenx/12, screeny+80));
			gl::disableAlphaBlending();
		}
		int xx = screenx*3/4;
		if ( searching == 0 && mousex > xx - screenx/12 && mousex < xx + screenx/12 )
		{
			gl::enableAlphaBlending();
			gl::color(0.4f, 0.4f, 1.0f, 0.5f);
			gl::drawSolidRect(Rectf(xx-screenx/12, screeny+20, xx+screenx/12, screeny+80));
			gl::disableAlphaBlending();
		}

		if ( mousex < screenx/12 ) // help
		{
			gl::enableAlphaBlending();
			gl::color(0.4f, 0.4f, 1.0f, 0.5f);
			gl::drawSolidRect(Rectf(0, screeny+20, screenx/12, screeny+80));
			gl::disableAlphaBlending();
		}
		else if ( mousex > screenx-screenx/12 )
		{
			gl::enableAlphaBlending();
			gl::color(0.4f, 0.4f, 1.0f, 0.5f);
			gl::drawSolidRect(Rectf(screenx-screenx/12, screeny+20, screenx, screeny+80));
			gl::disableAlphaBlending();
		}
	}
	if ( (searching == 4 || searching == 5) && mousex > screenx*3/4 - screenx/11 && mousex < screenx*3/4 + screenx/11
		&& mousey > screeny+10 && mousey < realscreeny)
	{
		int xx = screenx*3/4;
		gl::enableAlphaBlending();
		gl::color(0.4f, 0.4f, 1.0f, 0.5f);
		gl::drawSolidRect(Rectf(xx-screenx/11, screeny+10, xx+screenx/11, screeny+100));
		gl::disableAlphaBlending();
	}
	else if ( help && mousey > screeny/2+144 && mousey < screeny/2+194 && mousex > screenx/2-100 && mousex < screenx/2+100 )
	{
		gl::enableAlphaBlending();
		gl::color(0.4f, 0.4f, 1.0f, 0.5f);
		gl::drawSolidRect(Rectf(screenx/2-100, screeny/2+144, screenx/2+100, screeny/2+194));
		gl::disableAlphaBlending();
	}
}

CINDER_APP_NATIVE( ProjectileApp, RendererGl )

void ProjectileApp::loadSprites()
{
	tex[0] = gl::Texture( loadImage( loadResource( grass_, "IMAGE" ) ) );
	tex[1] = gl::Texture( loadImage( loadResource( sand_, "IMAGE" ) ) );
	tex[2] = gl::Texture( loadImage( loadResource( rock_, "IMAGE" ) ) );
	tex[3] = gl::Texture( loadImage( loadResource( ball_, "IMAGE" ) ) );
	tex[4] = gl::Texture( loadImage( loadResource( windarrow_, "IMAGE" ) ) );
}

void ProjectileApp::generateCourse()
{
	for( int i = 0; i < plats.size(); i++ ) // remove old platforms without leaking memory or leaving dangling pointers
		delete plats[i];
	plats.clear();

	// set default values:
	searching = 0;
	delete sweeper; sweeper = NULL;
	delete storedsweeper; storedsweeper = NULL;
	
	aiming = true;
	angle = 45.0f;
	speed = 60.0f;

	plats.push_back( new Plat( Rectf( 0, 0, 148, 148 ), randomInt(0, 2) ) ); // this platform outlines the wind indicator in the upper right corner of the screen

	// create all other platforms that form the course
	int xx = 0;
	int yy = 512;
	int txx = randomInt(0,2);
	plats.push_back( new Plat( Rectf( 0, 0, 25, 512), txx ) ); // left wall
	for(;;)
	{
		if ( yy+512 < screeny )
		{
			plats.push_back( new Plat( Rectf( 0, yy, 25, yy+512), txx ) ); // left wall
			yy += 512;
		}
		else
		{
			plats.push_back( new Plat( Rectf( 0, yy, 25, screeny), txx ) ); // left wall
			break;
		}
	}

	txx = randomInt(0,2);
	plats.push_back( new Plat( Rectf( screenx-25, 0, screenx, 512), txx ) ); // right wall
	yy = 512;
	for(;;)
	{
		if ( yy+512 < screeny )
		{
			plats.push_back( new Plat( Rectf( screenx-25, yy, screenx, yy+512), txx ) ); // right wall
			yy += 512;
		}
		else
		{
			plats.push_back( new Plat( Rectf( screenx-25, yy, screenx, screeny), txx ) ); // right wall
			break;
		}	
	}

	txx = randomInt(0, 2);
	plats.push_back( new Plat( Rectf( 25, 0, 25+512, 25), txx ) ); // ceiling
	xx = 25+512;
	for(;;)
	{
		if ( xx+512 < screenx-25 )
		{
			plats.push_back( new Plat( Rectf( xx, 0, xx+512, 25), txx ) ); // ceiling
			xx += 512;
		}
		else
		{
			plats.push_back( new Plat( Rectf( xx, 0, screenx-25, 25), txx ) ); // ceiling
			break;
		}	
	}

	// make all ground platform pieces
	yy = screeny - randomInt( 100, 300 );
	xx = randomInt(100, 250);
	plats.push_back( new Plat( Rectf( 0, yy, xx, screeny), randomInt(0, 2) ) ); // initial ground

	int bx = randomInt(25+20, xx-30); // place the ball in a semi-random position
	ballrect = Rectf( bx-5, yy-10, bx+5, yy );

	int oldxx = xx;
	for(;;)
	{
		xx += randomInt(100,512);
		if ( xx > screenx )
			xx = screenx;

		// ensure the ground platform pieces do not stray too far to the edges of the screen:
		if ( yy < screeny-50  && yy > 111 )
			yy += (-100+randomInt(0,200));
		else if ( yy >= screeny-50 )
			yy -= randomInt(20, 150);
		else if ( yy <= 111 )
			yy += randomInt(20, 150);
		if ( yy > screeny-30 )
			yy = screeny-30;
		if ( yy < 100 )
			yy = 100;

		if ( screeny-yy > 512 )
		{
			plats.push_back( new Plat( Rectf( oldxx, yy, xx, yy+512), randomInt(0, 2) ) );
			txx = plats[plats.size()-1]->texnum;
			plats.push_back( new Plat( Rectf( oldxx, yy+512, xx, screeny), txx ) );
		}
		else
			plats.push_back( new Plat( Rectf( oldxx, yy, xx, screeny), randomInt(0, 2) ) );
	
		oldxx = xx;
		if ( xx == screenx )
			break;
	}


	// place the target in a semi-random position
	targetx = screenx-25-23 - randomInt( 10, 50 );
	targety = screeny-23;
	while( collblock( Rectf( targetx-23, targety-23, targetx+23, targety+23 ) ) != -1 )
		targety -= 5;
	targety -= randomInt(10, 50);


	// create random floating blocks that do not collide with other blocks
	int num = randomInt(1,3);
	int width, height;
	for( int i = 0; i < num; i++ )
	{
		xx = randomInt(25, screenx-25);
		yy = randomInt(25, screeny-100);
		
		width = randomInt(64, 256);
		height = randomInt(64, 256);

		if ( collblock( Rectf( xx-20, yy-20, xx+width+20, yy+height+20 ) ) != -1 )
		{	i--; continue; }

		if ( collision( Rectf( xx-10, yy-10, xx+width+10, yy+height+20 ), ballrect ) )
		{	i--; continue; }

		if ( collision( Rectf( xx-10, yy-10, xx+width+10, yy+height+20 ), Rectf( targetx-23, targety-23, targetx+23, targety+23 ) ) )
		{	i--; continue; }

		plats.push_back( new Plat( Rectf( xx, yy, xx+width, yy+height ), randomInt(0, 2) ) );
	}

	// set default values:
	balldrawrect = ballrect;
	origballrect = ballrect;

	hit = 0;
	strokes = 0;

	for( int k = 0; k < 10; k++ )
		ballshadow[k] = Rectf( -20-k, 0, -10-k, 10 ); // initialize the ball's trailing motion off-screen

	// set random wind values
	windangle = randomInt(0, 359);
	windstrength = randomInt(1,30);
	xwind = abs((windstrength/1000.0f)*cos(0.0174533f*windangle));
	ywind = abs((windstrength/1000.0f)*sin(0.0174533f*angle));
	if ( windangle > 180.0f )
		ywind *= -1.0f;
	if ( windangle > 90.0f && windangle < 270.0f )
		xwind *= -1.0f;
	updateWindFbo(); // draw the texture for the wind's arrow indicator and text
	
	updateMenuFbo(); // draw the texture for the bottom menu bar

	if ( !isFrameRateEnabled() ) // initialize framerate to 60 frames per second
		setFrameRate(60);
}

bool ProjectileApp::collision( Rectf r1, Rectf r2 )
{
	// returns true if the two rectangles (given as arguments) collide with each other, false otherwise
	return !( r1.x2 <= r2.x1 || r1.x1 >= r2.x2 || r1.y1 >= r2.y2 || r1.y2 <= r2.y1 );
}

int ProjectileApp::collblock( Rectf r )
{
	// if the Rectf argument r collides with any platform, return the type of platform (0:grass/1:sand/2:rock) as an int. Return -1 otherwise
	for( int i = 0; i < plats.size(); i++ )
	{
		if ( collision( r, plats[i]->rect ) )
			return plats[i]->texnum;
	}
	return -1;
}

void ProjectileApp::updateWindFbo()
{
	// draw the texture of the wind's arrow indicator and text. Store it for quick rendering
	Area a = gl::getViewport();
	gl::pushMatrices();

	windfbo.bindFramebuffer();
	gl::setViewport( windfbo.getBounds() );
	gl::setMatricesWindow( windfbo.getSize(), false );
	
	gl::clear( Color(1,1,1) );
	//gl::clear( Color( 0.6f, 0.8f, 1.0f ) );

	gl::color(1,1,1);
	gl::pushMatrices();
	Matrix44f m;
	m.setToIdentity();
	m.translate( Vec3f(64, 64, 0) );
	m.scale( Vec2f( windstrength/15.0f, windstrength/15.0f ) );
	m.rotate( Vec3f::zAxis(), windangle*0.0174533f );
	gl::multModelView( m );

	gl::draw( tex[4], Area(0,0,32,32), Rectf(-16, -16, 16, 16) );

	gl::popMatrices();

	gl::enableAlphaBlending();
	gl::drawString( "Wind: " + toString(windstrength), Vec2f(10,10), black, fo );
	gl::disableAlphaBlending();

	windfbo.unbindFramebuffer();

	gl::popMatrices();
	gl::setViewport( a );
}

void ProjectileApp::updateMenuFbo()
{
	// draw the texture of the bottom menu bar. Store it for quick rendering
	Area a = gl::getViewport();
	gl::pushMatrices();

	menufbo.bindFramebuffer();
	gl::setViewport(menufbo.getBounds());
	gl::setMatricesWindow(menufbo.getSize(), false);

	gl::clear(Color(0.6f, 0.8f, 1.0f));

	gl::color(0, 0, 0);
	gl::drawSolidRect(Rectf(0, 0, screenx, 10));

	int hwidth = screenx/12;

	if ( !help )
	{
		if ( searching != 1 )
		{
			int xx = screenx/4;
		

			gl::drawStrokedRect(Rectf(xx-hwidth, 20, xx+hwidth, 80));

			xx = screenx/2;
			gl::drawStrokedRect(Rectf(xx-hwidth, 20, xx+hwidth, 80));

			gl::drawStrokedRect(Rectf(0, 20, screenx/12, 80));
			gl::drawStrokedRect(Rectf(screenx-screenx/12, 20, screenx, 80));

			gl::enableAlphaBlending();
			xx = screenx/4;
			gl::drawStringCentered("Reset", Vec2f(xx, 30), black, fo);
			gl::drawStringCentered("(R)", Vec2f(xx, 56), black, fo);
			xx = screenx/2;
			gl::drawStringCentered("New Course", Vec2f(xx, 30), black, fo);
			gl::drawStringCentered("(N)", Vec2f(xx, 56), black, fo);

			gl::drawStringCentered("?", Vec2f(screenx/24, 30), black, fo);
			gl::drawStringCentered("(H)", Vec2f(screenx/24, 56), black, fo);

			gl::drawStringCentered("Quit", Vec2f(screenx-screenx/24, 30), black, fo);
			gl::drawStringCentered("(ESC)", Vec2f(screenx-screenx/24, 56), black, fo);
			gl::disableAlphaBlending();

			xx = screenx*3/4;
			if ( searching == 0 ) // run solver algorithm
			{
				gl::drawStrokedRect(Rectf(xx-hwidth, 20, xx+hwidth, 80));

				gl::enableAlphaBlending();
				gl::drawStringCentered("Run Solver", Vec2f(xx, 30), black, fo);
				gl::drawStringCentered("Algorithm (S)", Vec2f(xx, 56), black, fo);
				gl::disableAlphaBlending();
			}
			else if ( searching == 4 ) // solution found!
			{
				gl::drawStrokedRect(Rectf(xx-screenx/11, 10, xx+screenx/11, 100));
				gl::enableAlphaBlending();
				gl::drawStringCentered("Solution Found!", Vec2f(xx, 17), black, fo);
				gl::drawStringCentered("Speed: " + toString(tSpeed), Vec2f(xx, 43), black, fo);
				gl::drawStringCentered("Angle: " + toString(tAngle), Vec2f(xx, 69), black, fo);
				gl::disableAlphaBlending();
			}
			else if ( searching == 5 ) // no solution
			{
				gl::drawStrokedRect(Rectf(xx-screenx/11, 10, xx+screenx/11, 100));
				gl::enableAlphaBlending();
				gl::drawStringCentered("No One-Shot Solution", Vec2f(xx, 17), black, fo);
				gl::drawStringCentered("Closest Speed: " + toString(tSpeed), Vec2f(xx, 43), black, fo);
				gl::drawStringCentered("Closest Angle: " + toString(tAngle), Vec2f(xx, 69), black, fo);
				gl::disableAlphaBlending();
			}
		}
	}
	else
	{
		Font bigfo = Font("Lucida Console", 54);
		gl::enableAlphaBlending();
		gl::drawStringCentered("Created by Briley Cantwell", Vec2f(screenx/2, 38), black, bigfo);
		gl::disableAlphaBlending();
	}

	menufbo.unbindFramebuffer();

	gl::popMatrices();
	gl::setViewport(a);


}

void ProjectileApp::makeHelpFbo()
{
	// draw the texture of the help pop-up. Store it for quick rendering
	Area a = gl::getViewport();
	gl::pushMatrices();

	helpfbo.bindFramebuffer();
	gl::setViewport(helpfbo.getBounds());
	gl::setMatricesWindow(helpfbo.getSize(), false);

	gl::clear(Color(0.0f,0.75f,1.0f));
	gl::color(1,1,1);

	Font newfo = Font("Lucida Console", 26);

	gl::enableAlphaBlending();
	gl::drawStringCentered("Use the arrow keys to aim for the target", Vec2f(256, 50), black, newfo);
	gl::drawStringCentered("Press SPACE to launch the projectile", Vec2f(256, 76), black, newfo);
	
	int xx = 85;
	int yy = 210;
	gl::drawStringCentered("Rock", Vec2f(xx, yy + 40), black, newfo);
	gl::drawStringCentered("(High Bounce)", Vec2f(xx, yy + 66), black, newfo);
	xx = 256;
	gl::drawStringCentered("Grass", Vec2f(xx, yy + 40), black, newfo);
	gl::drawStringCentered("(Medium Bounce)", Vec2f(xx, yy + 66), black, newfo);
	xx = 512-85;
	gl::drawStringCentered("Sand", Vec2f(xx, yy + 40), black, newfo);
	gl::drawStringCentered("(Low Bounce)", Vec2f(xx, yy + 66), black, newfo);

	gl::drawStringCentered("OK (SPACE)", Vec2f(256, yy + 210), black, newfo);
	gl::disableAlphaBlending();

	xx = 85;
	gl::draw( tex[2], Area(0,0,64,64), Rectf(xx-32, yy-32, xx+32, yy+32) );
	xx = 256;
	gl::draw( tex[0], Area(0,0,64,64), Rectf(xx-32, yy-32, xx+32, yy+32) );
	xx = 512-85;
	gl::draw( tex[1], Area(0,0,64,64), Rectf(xx-32, yy-32, xx+32, yy+32) );

	gl::color(0,0,0);
	gl::drawStrokedRect( Rectf(256-100, yy + 190, 256+100, yy + 240 ));

	helpfbo.unbindFramebuffer();

	gl::popMatrices();
	gl::setViewport(a);
}

void ProjectileApp::reset()
{
	// return the ball to its original position
	ballrect = origballrect;
	balldrawrect = origballrect;

	delete sweeper; sweeper = NULL;
	delete storedsweeper; storedsweeper = NULL;
	
	aiming = true;
	angle = 45.0f;
	speed = 60.0f;
	if ( searching == 4 || searching == 5 )
	{
		angle = tAngle;
		speed = tSpeed;
	}

	hit = 0;
	strokes = 0;

	for( int k = 0; k < 10; k++ )
		ballshadow[k] = Rectf( -20-k, 0, -10-k, 10 );
}

void Sweeper::simulate()
{
// quickly runs through the ball's flight path to determine if the ball will hit the target
while ( true )
{
	int tx = sweepercollblock( ballrect + Vec2f( xvel, yvel ) ); // determine if there is a collision with a platform in the ball's immediate path
	if ( tx != -1 ) // there is a collision
	{
		int collx = sweepercollblock( ballrect + Vec2f( xvel, 0 ) ); // is the collision in the x (horizontal) direction?
		int colly = sweepercollblock( ballrect + Vec2f( 0, yvel ) ); // is the collision in the y (vertical) direction?

		if ( (collx != -1 && colly != -1) || (collx == -1 && colly == -1) ) // collision is in both x and y directions (it's a corner bounce)
		{
			// the ball moves to contact with the platform, then reverses velocities in the x and y axes.
			float i;
			for( i = 0.05f; i < 1.0f; i += 0.05f )
			{
				if ( sweepercollblock( ballrect + Vec2f( xvel*i, yvel*i ) ) != -1 )
					break;
			}
			ballrect += Vec2f( xvel*(i-0.05f), yvel*(i-0.05f) );
			
			if ( tx == 0 ) // grass: medium bounce
			{
				xvel *= -0.5f;
				yvel *= -0.5f;
			}
			else if ( tx == 1 ) // sand: low/weak bounce
			{
				xvel *= -0.25f;
				yvel *= -0.25f;
			}
			else if ( tx == 2 ) // rock: high bounce
			{
				xvel *= -0.9f;
				yvel *= -0.9f;
			}
		}
		else if ( collx != -1 ) // collision is only in the x direction (horizontal bounce)
		{
			// the ball moves to contact with the platform, then reverses velocity in the x axis only.
			float i;
			for( i = 0.05f; i < 1.0f; i += 0.05f )
			{
				if ( sweepercollblock( ballrect + Vec2f( xvel*i, 0 ) ) != -1 )
					break;
			}
			ballrect += Vec2f( xvel*(i-0.05f), 0 );

			if ( tx == 0 ) // grass: medium bounce
			{
				xvel *= -0.5f;
				//yvel *= 0.5f;
			}
			else if ( tx == 1 ) // sand: low/weak bounce
			{
				xvel *= -0.25f;
				//yvel *= 0.25f;
			}
			else if ( tx == 2 ) // rock: high bounce
			{
				xvel *= -0.9f;
				//yvel *= 0.9f;
			}
		}
		else // if ( colly ) // collision is only in the y direction (vertical bounce)
		{
			// the ball moves to contact with the platform, then reverses velocity in the y axis only.
			float i;
			for( i = 0.05f; i < 1.0f; i += 0.05f )
			{
				if ( sweepercollblock( ballrect + Vec2f( 0, yvel*i ) ) != -1 )
					break;
			}
			ballrect += Vec2f( 0, yvel*(i-0.05f) );

			if ( tx == 0 ) // grass: medium bounce
			{
				//xvel *= 0.5f;
				yvel *= -0.5f;
			}
			else if ( tx == 1 ) // sand: low/weak bounce
			{
				//xvel *= 0.25f;
				yvel *= -0.25f;
			}
			else if ( tx == 2 ) // rock: high bounce
			{
				//xvel *= 0.9f;
				yvel *= -0.9f;
			}
		}
	}
	else // there is no collision in the ball's immediate path
		ballrect += Vec2f( xvel, yvel ); // update the ball's position based on its velocities in the x and y direction

	// apply gravity effects
	yvel += 0.1f; // slightly increase the ball's y velocity downward every frame to simulate gravity
	// wind
	xvel += xwind; // slightly change the ball's velocities in the x and y direction to simulate wind
	yvel += ywind;

	for( int i = 0; i < 9; i++ ) // update the ball's shadow for easier motion viewing
		ballshadow[i] = ballshadow[i+1];
	ballshadow[9] = balldrawrect;

	balldrawrect = Rectf( (int) (ballrect.x1+0.5f), (int) (ballrect.y1+0.5f), (int) (ballrect.x2+0.5f), (int) (ballrect.y2+0.5f) );
		
	for( int i = 0; i < 9; i++ ) // if the ball has not moved in the last 10 frames, stop updating its position and return controls to the user to aim the ball again
	{
		if ( ballshadow[i].x1 != ballshadow[i+1].x1 ||
			ballshadow[i].y1 != ballshadow[i+1].y1 )
			break;
		if ( i == 8 )
		{
			for( int k = 0; k < 10; k++ )
				ballshadow[k] = Rectf( -20-k, 0, -10-k, 10 );

			this->finished = true;
			
			break;
		}
	}

	// check if the ball has hit the target
	float hyp = sqrt(((ballrect.x1+5.0f)-targetx)*((ballrect.x1+5.0f)-targetx)
		+ ((ballrect.y1+5.0f)-targety)*((ballrect.y1+5.0f)-targety));
	if ( hyp <= 26.0f )
	{
		successful = true; // successful hit
		finished = true;
	}
	else if ( closest < -90.0f || hyp < closest )
		closest = hyp; // store the closest distance the ball has been to the target

	if ( finished )
		break;
}



}

bool Sweeper::sweepercollision( Rectf r1, Rectf r2 )
{
	// returns true if the two rectangles (given as arguments) collide with each other, false otherwise
	return !( r1.x2 <= r2.x1 || r1.x1 >= r2.x2 || r1.y1 >= r2.y2 || r1.y2 <= r2.y1 );
}

int Sweeper::sweepercollblock( Rectf r )
{
	// if the Rectf argument r collides with any platform, return the type of platform (0:grass/1:sand/2:rock) as an int. Return -1 otherwise
	for( int i = 0; i < (*vp).size(); i++ )
	{
		if ( sweepercollision( r, (*vp)[i]->rect ) )
			return (*vp)[i]->texnum;
	}
	return -1;
}




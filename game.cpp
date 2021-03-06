#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>
#include <chrono>
#include <stdio.h>
#include <Windows.h>

using namespace std;

// create global variables
int screenWidth = 120;			// Console Screen Size X (columns)
int screenHeight = 40;			// Console Screen Size Y (rows)
int mapWidth = 16;				// World Dimensions
int mapHeight = 16;

float playerX = 14.7f;			// Player Start Position
float playerY = 5.09f;
float playerA = 0.0f;			// Player Start Rotation
float FOV = 3.14159f / 4.0f;	// Field of View
float depth = 16.0f;			// Maximum rendering distance
float speed = 3.0f;			// Walking Speed

int main(){
	// Create Screen Buffer
	wchar_t* screen = new wchar_t[screenWidth * screenHeight];
	HANDLE heightConsole = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
	SetConsoleActiveScreenBuffer(heightConsole);
	DWORD dwBytesWritten = 0;

	// Create Map of world space # = wall block, . = space
	wstring map;
	map += L"################";
	map += L"#..............#";
	map += L"#..##########..#";
	map += L"#........#..#..#";
	map += L"#...........#..#";
	map += L"#..............#";
	map += L"#..##########..#";
	map += L"#..#....#...#..#";
	map += L"#..#....#...#..#";
	map += L"#..............#";
	map += L"#..............#";
	map += L"#..##########..#";
	map += L"#..............#";
	map += L"#..............#";
	map += L"#..............#";
	map += L"################";

	auto tp1 = chrono::system_clock::now();
	auto tp2 = chrono::system_clock::now();

	while (1){
		// We'll need time differential per frame to calculate modification
		// to movement speeds, to ensure consistant movement, as ray-tracing
		// is non-deterministic
		tp2 = chrono::system_clock::now();
		chrono::duration<float> elapsedTime = tp2 - tp1;
		tp1 = tp2;
		float fElapsedTime = elapsedTime.count();


		// Handle CCW Rotation
		if (GetAsyncKeyState((unsigned short)'A') & 0x8000) 
		{
			playerA -= (speed * 0.75f) * fElapsedTime;
		}

		// Handle CW Rotation
		if (GetAsyncKeyState((unsigned short)'D') & 0x8000) 
		{
			playerA += (speed * 0.75f) * fElapsedTime;
		}

		// Handle Forwards movement & collision
		if (GetAsyncKeyState((unsigned short)'W') & 0x8000)
		{
			playerX += sinf(playerA) * speed * fElapsedTime;;
			playerY += cosf(playerA) * speed * fElapsedTime;;
			if (map.c_str()[(int)playerX * mapWidth + (int)playerY] == '#')
			{
				playerX -= sinf(playerA) * speed * fElapsedTime;;
				playerY -= cosf(playerA) * speed * fElapsedTime;;
			}
		}

		// Handle backwards movement & collision
		if (GetAsyncKeyState((unsigned short)'S') & 0x8000)
		{
			playerX -= sinf(playerA) * speed * fElapsedTime;;
			playerY -= cosf(playerA) * speed * fElapsedTime;;
			if (map.c_str()[(int)playerX * mapWidth + (int)playerY] == '#')
			{
				playerX += sinf(playerA) * speed * fElapsedTime;;
				playerY += cosf(playerA) * speed * fElapsedTime;;
			}
		}

		for (int x = 0; x < screenWidth; x++)
		{
			// For each column, calculate the projected ray angle into world space
			float rayAngle = (playerA - FOV / 2.0f) + ((float)x / (float)screenWidth) * FOV;

			// Find distance to wall
			float stepSize = 0.01f;		  // Increment size for ray casting, decrease to increase										
			float distanceToWall = 0.0f; //                                      resolution

			bool bHitWall = false;		// Set when ray hits wall block
			bool bBoundary = false;		// Set when ray hits boundary between two wall blocks

			float fEyeX = sinf(rayAngle); // Unit vector for ray in player space
			float fEyeY = cosf(rayAngle);

			// Incrementally cast ray from player, along ray angle, testing for 
			// intersection with a block
			while (!bHitWall && distanceToWall < depth)
			{
				distanceToWall += stepSize;
				int nTestX = (int)(playerX + fEyeX * distanceToWall);
				int nTestY = (int)(playerY + fEyeY * distanceToWall);

				// Test if ray is out of bounds
				if (nTestX < 0 || nTestX >= mapWidth || nTestY < 0 || nTestY >= mapHeight)
				{
					bHitWall = true;			// Just set distance to maximum depth
					distanceToWall = depth;
				}
				else
				{
					// Ray is inbounds so test to see if the ray cell is a wall block
					if (map.c_str()[nTestX * mapWidth + nTestY] == '#')
					{
						// Ray has hit wall
						bHitWall = true;

						// To highlight tile boundaries, cast a ray from each corner
						// of the tile, to the player. The more coincident this ray
						// is to the rendering ray, the closer we are to a tile 
						// boundary, which we'll shade to add detail to the walls
						vector<pair<float, float>> p;

						// Test each corner of hit tile, storing the distance from
						// the player, and the calculated dot product of the two rays
						for (int tx = 0; tx < 2; tx++)
							for (int ty = 0; ty < 2; ty++)
							{
								// Angle of corner to eye
								float vy = (float)nTestY + ty - playerY;
								float vx = (float)nTestX + tx - playerX;
								float d = sqrt(vx * vx + vy * vy);
								float dot = (fEyeX * vx / d) + (fEyeY * vy / d);
								p.push_back(make_pair(d, dot));
							}

						// Sort Pairs from closest to farthest
						sort(p.begin(), p.end(), [](const pair<float, float>& left, const pair<float, float>& right) {return left.first < right.first; });

						// First two/three are closest (we will never see all four)
						float fBound = 0.01;

						if ((acos(p.at(0).second) < fBound) || (acos(p.at(1).second) < fBound) || (acos(p.at(2).second) < fBound)) {
							bBoundary = true;
						}
					}
				}
			}

			// Calculate distance to ceiling and floor
			int nCeiling = (float)(screenHeight / 2.0) - screenHeight / ((float)distanceToWall);
			int nFloor = screenHeight - nCeiling;

			// Shader walls based on distance
			short nShade = ' ';
			if (distanceToWall <= depth / 4.0f) {
				nShade = 0x2588;	// Very close	
			} else if (distanceToWall < depth / 3.0f) {
				nShade = 0x2593;
			} else if (distanceToWall < depth / 2.0f){
				nShade = 0x2592;
			} else if (distanceToWall < depth){
				nShade = 0x2591;
			} else{
				nShade = ' '; // Too far away
			}


			if (bBoundary) {
				nShade = ' '; // Black it out
			}

			for (int y = 0; y < screenHeight; y++)
			{
				// Each Row
				if (y <= nCeiling) {
					screen[y * screenWidth + x] = ' ';
				} else if (y > nCeiling && y <= nFloor) {
					screen[y * screenWidth + x] = nShade;
				} else {	// Floor
					// Shade floor based on distance
					float b = 1.0f - (((float)y - screenHeight / 2.0f) / ((float)screenHeight / 2.0f));
					if (b < 0.25) {
						nShade = '#';
					} else if (b < 0.5) {
						nShade = 'x';
					} else if (b < 0.75) {
						nShade = '.';
					} else if (b < 0.9) {
						nShade = '-';
					} else {
						nShade = ' ';
					}

					screen[y * screenWidth + x] = nShade;
				}
			}
		}

		// Display Stats
		swprintf_s(screen, 40, L"X=%3.2f, Y=%3.2f, A=%3.2f FPS=%3.2f ", playerX, playerY, playerA, 1.0f / fElapsedTime);

		// Display Map
		for (int nx = 0; nx < mapWidth; nx++) {
			for (int ny = 0; ny < mapWidth; ny++) {
				screen[(ny + 1) * screenWidth + nx] = map[ny * mapWidth + nx];
			}
		}

		screen[((int)playerX + 1) * screenWidth + (int)playerY] = 'P';

		// Display Frame
		screen[screenWidth * screenHeight - 1] = '\0';
		WriteConsoleOutputCharacter(heightConsole, screen, screenWidth * screenHeight, { 0,0 }, &dwBytesWritten);
	}

	return 0;
}

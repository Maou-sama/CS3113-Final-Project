#ifdef _WINDOWS
	#include <GL/glew.h>
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <stdio.h>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <assert.h>
#include "ShaderProgram.h"
#include "Matrix.h"
#include <vector>
#include <SDL_mixer.h>

#ifdef _WINDOWS
	#define RESOURCE_FOLDER ""
#else
	#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

#define FIXED_TIMESTEP 0.0166666f
#define PI 3.14159265f

//Variables
SDL_Window* displayWindow;
Matrix projectionMatrix;
Matrix modelMatrix;
Matrix viewMatrix;
const Uint8 *keys = SDL_GetKeyboardState(NULL);
bool done;
float ticks;
float elapsed;
float lastFrameTicks = 0.0f;
enum GameMode { STATE_MAIN_MENU, STATE_GAME_LEVEL, STATE_GAME_OVER };
enum EntityType { ENTITY_PLAYER, ENTITY_ENEMY, ENTITY_BOSS_BULLET, ENTITY_BOSS_BULLET_2, ENTITY_BOSS_BULLET_3, ENTITY_BOSS, ENTITY_STAR, ENTITY_BULLET };
GameMode state;

//Class for Vector 3
class Vector3 {
public:
	Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

	float x;
	float y;
	float z;
};

//Class for sprite from sprite sheet
class SheetSprite {
public:
	SheetSprite() : textureID(NULL), u(NULL), v(NULL), width(NULL), height(NULL), size(NULL) {}
	SheetSprite(unsigned int textureID, float u, float v, float width, float height, float
		size) : textureID(textureID), u(u), v(v), width(width), height(height), size(size) {}

	void Draw(ShaderProgram *program);

	float size;
	unsigned int textureID;
	float u;
	float v;
	float width;
	float height;
};

//Draw function for the sprite
void SheetSprite::Draw(ShaderProgram *program) {
	glBindTexture(GL_TEXTURE_2D, textureID);
	GLfloat texCoords[] = {
		u, v + height,
		u + width, v,
		u, v,
		u + width, v,
		u, v + height,
		u + width, v + height
	};
	float aspect = width / height;
	float vertices[] = {
		-0.5f * size * aspect, -0.5f * size,
		0.5f * size * aspect, 0.5f * size,
		-0.5f * size * aspect, 0.5f * size,
		0.5f * size * aspect, 0.5f * size,
		-0.5f * size * aspect, -0.5f * size,
		0.5f * size * aspect, -0.5f * size };

	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
	glEnableVertexAttribArray(program->positionAttribute);

	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
	glEnableVertexAttribArray(program->texCoordAttribute);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(program->positionAttribute);
	glDisableVertexAttribArray(program->texCoordAttribute);
}

//Loading texture from file
GLuint LoadTexture(const char *filePath) {
	int w, h, comp;
	unsigned char* image = stbi_load(filePath, &w, &h, &comp, STBI_rgb_alpha);
	if (image == NULL) {
		std::cout << "Unable to load image. Make sure the path is correct\n";
		assert(false);
	}
	GLuint retTexture;
	glGenTextures(1, &retTexture);
	glBindTexture(GL_TEXTURE_2D, retTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	stbi_image_free(image);
	return retTexture;
}

//Function to draw text from texture file
void DrawText(ShaderProgram *program, int fontTexture, std::string text, float size, float spacing) {
	float texture_size = 1.0 / 16.0f;
	std::vector<float> vertexData;
	std::vector<float> texCoordData;

	for (int i = 0; i < text.size(); i++) {
		int spriteIndex = (int)text[i];
		float texture_x = (float)(spriteIndex % 16) / 16.0f;
		float texture_y = (float)(spriteIndex / 16) / 16.0f;

		vertexData.insert(vertexData.end(), {
			((size + spacing) * i) + (-0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
		});
		texCoordData.insert(texCoordData.end(), {
			texture_x, texture_y,
			texture_x, texture_y + texture_size,
			texture_x + texture_size, texture_y,
			texture_x + texture_size, texture_y + texture_size,
			texture_x + texture_size, texture_y,
			texture_x, texture_y + texture_size,
		});
	}

	glBindTexture(GL_TEXTURE_2D, fontTexture);

	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
	glEnableVertexAttribArray(program->positionAttribute);

	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
	glEnableVertexAttribArray(program->texCoordAttribute);

	glDrawArrays(GL_TRIANGLES, 0, 6 * text.size());

	glDisableVertexAttribArray(program->positionAttribute);
	glDisableVertexAttribArray(program->texCoordAttribute);
}

//Linear Interpolation
float lerp(float v0, float v1, float t) {
	return (1.0 - t) * v0 + t * v1;
}

//Map value
float mapValue(float value, float srcMin, float srcMax, float dstMin, float dstMax) {
	float retVal = dstMin + ((value - srcMin) / (srcMax - srcMin) * (dstMax - dstMin));

	if (retVal < dstMin) {
		retVal = dstMin;
	}
	if (retVal > dstMax) {
		retVal = dstMax;
	}

	return retVal;
}

//Ease out for tweening
float easeOut(float from, float to, float time) {
	float oneMinusT = 1.0f - time;
	float tVal = 1.0f - (oneMinusT * oneMinusT * oneMinusT * oneMinusT * oneMinusT);
	return (1.0f - tVal) * from + tVal * to;
}

//Elastic ease out for tweening
float easeOutElastic(float from, float to, float time) {
	float p = 0.3f;
	float s = p / 4.0f;
	float diff = (to - from);
	return from + diff + (diff * pow(2.0f, -10.0f * time) * sin((time - s)*(2 * PI) / p));
}

//The base entity class
class Entity {
public:
	Entity() : position(Vector3(0.0f, 0.0f, 0.0f)), velocity(Vector3(0.0f, 0.0f, 0.0f)), isStatic(true), active(false) {}

	Entity(Vector3 pos, SheetSprite spr, EntityType et, bool isStatic, bool active)
		: position(pos), velocity(Vector3(0.0f, 0.0f, 0.0f)), sprite(spr), entityType(et), isStatic(isStatic), active(active) {}

	~Entity() {

	}

	void Update(float elapsed);
	void Render(Matrix& model, Matrix& view, ShaderProgram& program);

	SheetSprite sprite;
	Vector3 position;
	//Vector3 size;
	Vector3 velocity;
	EntityType entityType;
	bool isStatic;

	bool active;

	//Getting the top bottom left right position to check for collision
	float top()
	{
		return position.y + sprite.size / 2;
	}

	float bottom()
	{
		return position.y - sprite.size / 2;
	}

	float left()
	{
		return position.x - sprite.size * (sprite.width / sprite.height) / 2;
	}

	float right()
	{
		return position.x + sprite.size * (sprite.width / sprite.height) / 2;
	}


	bool checkCollision(Entity& entity)
	{
		if (top() >= entity.bottom() && bottom() <= entity.top() && left() < entity.right() && right() >= entity.left())
			return true;

		return false;
	}
};

//The player class inherited from the entity class
class Player : public Entity {
public:
	Player() : Entity(), ammo(0) {}
	Player(Vector3 pos, SheetSprite spr, EntityType et, bool isStatic, bool active, int ammo) : Entity(pos, spr, et, isStatic, active), ammo(ammo) {}

	void Render(Matrix& model, Matrix& view, ShaderProgram& program);

	int ammo; //Have variable to keep track of ammo
};

//The enemy class inherited from the entity class
class Enemy : public Entity {
public:
	Enemy() : Entity(), hp(0) {}
	Enemy(Vector3 pos, SheetSprite spr, EntityType et, bool isStatic, bool active, int hp, int spawnPosition) : Entity(pos, spr, et, isStatic, active), hp(hp), spawnPosition(spawnPosition) {}

	void Render(Matrix& model, Matrix& view, ShaderProgram& program);
	void Update(float elapsed);

	int hp; //amount of hp
	int spawnPosition; //which side of the screen is the enemy spawned: 1 is left 2 is right
};

//The boss class inherited from the entity class
class Boss : public Entity {
public:
	Boss() : Entity(), hp(0) {}
	Boss(Vector3 pos, SheetSprite spr, EntityType et, bool isStatic, bool active, int hp) : Entity(pos, spr, et, isStatic, active), hp(hp) {}

	void Render(Matrix& model, Matrix& view, ShaderProgram& program);
	void Update(float elapsed);

	int hp; //amount of hp
};

//More Variables (a bunch of timer to keep track of when to do something)
bool showStatistics;

int timer;
float gameSeconds = 0.0f;

int currentLevel;
bool showLevel;
bool spawnEnemy;

//Bullet Variables
int noOfBullet = 0;
int bossSpawnBulletTimer = 1;
int bossNotSpawnBulletTimer = 1;
bool spawnBullet = true;
int bossStraightBulletTimer = 1;
int bossDiagonalBulletTimer = 1;

//All the player, boss, enemies, background stars, bullets variables and their sprite
Player player;
Player player2;
int numPlayer;

Boss theBoss;
std::vector<Entity> stars;
std::vector<Entity> playerBullets;
std::vector<Entity> player2Bullets;
std::vector<Entity> bossBullets;
std::vector<Enemy> enemies;
SheetSprite spr_player;
SheetSprite spr_player_2;
SheetSprite spr_star;
SheetSprite spr_player_bullet;
SheetSprite spr_boss_bullet;
SheetSprite spr_enemy;

GLuint boss;
SheetSprite spr_boss;

//Screen shake variables
bool screenShake = false;
float screenShakeValue = 0.0f;
float screenShakeSpeed = 32.0f;
float screenShakeIntensity = 0.5f;

int shakeTimer = 0;

GLuint text;
GLuint menuText;

float starXPosition;

float animationTime = 0.0f;
float animationStart = 0.0f;
float animationEnd = 0.08f;
float animationValue;

//Game music
Mix_Music *menuMusic;
Mix_Music *gameMusic;
Mix_Chunk *shootingSound;

bool playMenuMusic = false;
bool playGameMusic = false;

//Timer to keep track of when to go to game over screen
int playerDeadTimer = 1;

int bossDeadTimer = 1;
bool bossSpawned = false;

//Update function for Entity
void Entity::Update(float elapsed) {
	if (active) {
		//Star will move down the screen and destroyed when out of screen
		if (entityType == ENTITY_STAR) {
			position.y += -5.0f * elapsed;

			if (position.y <= -4.0f) {
				active = false;
			}
		}

		//Player bullet will move up and destroyed when out of screen or when
		//collides with enemies destroying both
		if (entityType == ENTITY_BULLET) {
			position.y += 5.0f * elapsed;

			if (position.y >= 4.0f) {
				active = false;
			}

			for (size_t i = 0; i < enemies.size(); i++) {
				if (checkCollision(enemies[i])) {
					active = false;
					enemies[i].hp--;
					if (enemies[i].hp == 1) {
						enemies[i].hp--;
						enemies[i].active = false;
					}
				}
			}

			if (currentLevel == 3) {
				if (checkCollision(theBoss)) {
					active = false;
					theBoss.hp--;
					if (theBoss.hp == 1) {
						theBoss.hp--;
						theBoss.active = false;
					}
				}
			}
		}

		//Boss bullet type 1 will move down and destroyed when out of screen or when
		//collides with players destroying both
		if (entityType == ENTITY_BOSS_BULLET) {
			position.y -= 5.0f * elapsed;

			if (position.y <= -4.0f) {
				active = false;
			}

			if (player.active) {
				if (checkCollision(player)) {
					active = false;
					player.active = false;
					screenShake = true;
				}
			}

			if (numPlayer == 2) {
				if (player2.active) {
					if (checkCollision(player2)) {
						active = false;
						player2.active = false;
						screenShake = true;
					}
				}
			}
		}

		//Boss bullet type 2 will move in  a -120 angles and destroyed when out of screen or when
		//collides with players destroying both
		if (entityType == ENTITY_BOSS_BULLET_2) {
			position.x += cos(-(2 * PI / 3)) * 5.0f * elapsed;
			position.y += sin(-(2 * PI / 3)) * 5.0f * elapsed;

			if (position.y <= -4.0f) {
				active = false;
			}

			if (player.active) {
				if (checkCollision(player)) {
					active = false;
					player.active = false;
					screenShake = true;
				}
			}

			if (numPlayer == 2) {
				if (player2.active) {
					if (checkCollision(player2)) {
						active = false;
						player2.active = false;
						screenShake = true;
					}
				}
			}
		}

		//Boss bullet type 3 will move in  a -60 angles and destroyed when out of screen or when
		//collides with player destroying both
		if (entityType == ENTITY_BOSS_BULLET_3) {
			position.x += cos(-(PI / 3)) * 5.0f * elapsed;
			position.y += sin(-(PI / 3)) * 5.0f * elapsed;

			if (position.y <= -4.0f) {
				active = false;
			}

			if (player.active) {
				if (checkCollision(player)) {
					active = false;
					player.active = false;
					screenShake = true;
				}
			}

			if (numPlayer == 2) {
				if (player2.active) {
					if (checkCollision(player2)) {
						active = false;
						player2.active = false;
						screenShake = true;
					}
				}
			}
		}
	}
}

//How to Render Entity
void Entity::Render(Matrix& model, Matrix& view, ShaderProgram& program) {
	if (active) {
		if (entityType == ENTITY_BOSS_BULLET) {
			model.Identity();
			model.Translate(position.x, position.y, 0);
			model.Rotate(PI);
			program.SetModelviewMatrix(model * view);
			sprite.Draw(&program);
		}

		else if (entityType == ENTITY_BOSS_BULLET_2) {
			model.Identity();
			model.Translate(position.x, position.y, 0);
			model.Rotate(5 * PI / 6);
			program.SetModelviewMatrix(model * view);
			sprite.Draw(&program);
		}

		else if (entityType == ENTITY_BOSS_BULLET_3) {
			model.Identity();
			model.Translate(position.x, position.y, 0);
			model.Rotate(7 * PI / 6);
			program.SetModelviewMatrix(model * view);
			sprite.Draw(&program);
		}

		else {
			model.Identity();
			model.Translate(position.x, position.y, 0);
			program.SetModelviewMatrix(model * view);
			sprite.Draw(&program);
		}
	}
}

//How to render players
void Player::Render(Matrix& model, Matrix& view, ShaderProgram& program) {
	if (active) {
		model.Identity();
		model.Translate(position.x + 1.0f, position.y, 0.0f);
		program.SetModelviewMatrix(model*view);
		DrawText(&program, text, "Ammo: " + std::to_string(ammo), 0.3f, -.15f);

		model.Identity();
		model.Translate(position.x, position.y, 0);
		program.SetModelviewMatrix(model*view);
		sprite.Draw(&program);
	}
}

//How to render enemies
void Enemy::Render(Matrix& model, Matrix& view, ShaderProgram& program) {
	if (active) {
		model.Identity();
		model.Translate(position.x + 1.0f, position.y, 0.0f);
		program.SetModelviewMatrix(model*view);
		DrawText(&program, text, "HP: " + std::to_string(hp), 0.3f, -.15f);

		model.Identity();
		model.Translate(position.x, position.y, 0);
		program.SetModelviewMatrix(model*view);
		sprite.Draw(&program);
	}
}

//Update function for enemy
void Enemy::Update(float elapsed) {
	//Enemy move straight down for level 1
	if (currentLevel == 1) {
		position.y -= 2.0f * elapsed;
	}

	//Enemy on left move at -60 angles and enemy on right
	//move at -120 angles with faster speed
	else if (currentLevel == 2) {
		if (spawnPosition == 1) {
			position.x += cos(-(PI / 3)) * 3.0f * elapsed;
			position.y += sin(-(PI / 3)) * 3.0f * elapsed;
		}

		else if (spawnPosition == 2) {
			position.x += cos(-(2 * PI / 3)) * 3.5f * elapsed;
			position.y += sin(-(2 * PI / 3)) * 3.5f * elapsed;
		}
	}

	//if collides with players destroy them and shake screen
	if (checkCollision(player) && player.active) {
		player.active = false;
		screenShake = true;
	}

	if (numPlayer == 2) {
		if (checkCollision(player2) && player2.active) {
			player2.active = false;
			screenShake = true;
		}
	}
}

//How to render boss
void Boss::Render(Matrix& model, Matrix& view, ShaderProgram& program) {
	if (active) {
		model.Identity();
		model.Translate(position.x + 1.0f, position.y, 0.0f);
		program.SetModelviewMatrix(model*view);
		DrawText(&program, text, "HP: " + std::to_string(hp), 0.3f, -.15f);

		model.Identity();
		model.Translate(position.x, position.y, 0);
		program.SetModelviewMatrix(model*view);
		sprite.Draw(&program);
	}
}

void Boss::Update(float elapsed) {
	//Boss will move to a certain possition before stopping
	if (position.y >= 3.0f) {
		position.y -= 1.0f * elapsed;
	}

	//Destroy players when collides and shake screen
	if (checkCollision(player) && player.active) {
		player.active = false;
		screenShake = true;
	}
}

//Reset every variables function
void Reset() {
	timer = 0;
	gameSeconds = 0.0f;
	currentLevel = 1;
	showStatistics = false;
	showLevel = true;
	spawnEnemy = true;

	player = Player();
	player2 = Player();

	stars = {};
	playerBullets = {};
	player2Bullets = {};
	bossBullets = {};
	enemies = {};
	theBoss = Boss();
}

//Processing user input
void processInput(Player& player, Player& player2, float elapsed)
{
	switch (state) {
		//When player is in main menu
	case STATE_MAIN_MENU:
		if (keys[SDL_SCANCODE_F1]) {
			state = STATE_GAME_LEVEL;
			numPlayer = 1;
		}

		if (keys[SDL_SCANCODE_F2]) {
			state = STATE_GAME_LEVEL;
			numPlayer = 2;
		}

		if (keys[SDL_SCANCODE_ESCAPE]) {
			done = true;
		}
		break;

		//When player in game
	case STATE_GAME_LEVEL:
		//Turn on off statistics with H or J
		if (keys[SDL_SCANCODE_H]) {
			showStatistics = true;
		}

		if (keys[SDL_SCANCODE_J]) {
			showStatistics = false;
		}

		//Player movement input
		if (player.active) {
			if (keys[SDL_SCANCODE_LEFT])
			{
				if (player.position.x > -6.6f) {
					player.position.x += -0.25f;
				}
			}
			else if (keys[SDL_SCANCODE_RIGHT])
			{
				if (player.position.x < 6.6f) {
					player.position.x += 0.25f;
				}
			}

			if (keys[SDL_SCANCODE_UP])
			{
				if (player.position.y < 3.75f) {
					player.position.y += 0.25f;
				}
			}
			else if (keys[SDL_SCANCODE_DOWN])
			{
				if (player.position.y > -3.75f) {
					player.position.y += -0.25f;
				}
			}

			//Sound when shooting and deplete ammo
			if (keys[SDL_SCANCODE_SPACE]) {
				if (player.ammo > 0) {
					Mix_PlayChannel(-1, shootingSound, 0);
					playerBullets.push_back(Entity(Vector3(player.position.x, player.position.y + 0.5f, 0.0f), spr_player_bullet, ENTITY_BULLET, false, true));
					player.ammo--;
					noOfBullet++;
				}
			}
		}

		//Player 2 movement input
		if (player2.active) {
			if (keys[SDL_SCANCODE_A])
			{
				if (player2.position.x > -6.6f) {
					player2.position.x += -0.25f;
				}
			}
			else if (keys[SDL_SCANCODE_D])
			{
				if (player2.position.x < 6.6f) {
					player2.position.x += 0.25f;
				}
			}

			if (keys[SDL_SCANCODE_W])
			{
				if (player2.position.y < 3.75f) {
					player2.position.y += 0.25f;
				}
			}
			else if (keys[SDL_SCANCODE_S])
			{
				if (player2.position.y > -3.75f) {
					player2.position.y += -0.25f;
				}
			}

			//Sound when shooting and deplete ammo
			if (keys[SDL_SCANCODE_LCTRL]) {
				if (player2.ammo > 0) {
					Mix_PlayChannel(-1, shootingSound, 0);
					playerBullets.push_back(Entity(Vector3(player2.position.x, player2.position.y + 0.5f, 0.0f), spr_player_bullet, ENTITY_BULLET, false, true));
					player2.ammo--;
					noOfBullet++;
				}
			}
		}

		//Manually stop screen shake with E
		if (keys[SDL_SCANCODE_E]) {
			screenShake = false;
		}

		//Press Q to go to game over screen
		if (keys[SDL_SCANCODE_Q]) {
			state = STATE_GAME_OVER;
		}
		break;

		//When user in game over screen
	case STATE_GAME_OVER:
		if (keys[SDL_SCANCODE_RETURN]) {
			Reset();
			state = STATE_MAIN_MENU;
		}

		if (keys[SDL_SCANCODE_ESCAPE]) {
			done = true;
		}
		break;
	}
}

//Initial Setup
void Setup() {

	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1200, 900, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
	#ifdef _WINDOWS
		glewInit();
	#endif

	glViewport(0, 0, 1200, 900);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	projectionMatrix.SetOrthoProjection(-7.1f, 7.1f, -4.0f, 4.0f, -2.0f, 2.0f);

	state = STATE_MAIN_MENU;
	timer = 0;
	currentLevel = 1;
	showStatistics = false;
	showLevel = true;
	spawnEnemy = true;

	//Loading Sprite
	GLuint spriteSheetSpace = LoadTexture(RESOURCE_FOLDER"sheet.png");

	spr_player = SheetSprite(spriteSheetSpace, 325.0f / 1024.0f, 739.0f / 1024.0f, 98.0f / 1024.0f, 75.0f / 1024.0f, 0.5f);
	spr_player_2 = SheetSprite(spriteSheetSpace, 346.0f / 1024.0f, 75.0f / 1024.0f, 98.0f / 1024.0f, 75.0f / 1024.0f, 0.5f);
	spr_star = SheetSprite(spriteSheetSpace, 382.0f / 1024.0f, 814.0f / 1024.0f, 17.0f / 1024.0f, 17.0f / 1024.0f, 0.1f);
	spr_player_bullet = SheetSprite(spriteSheetSpace, 856.0f / 1024.0f, 775.0f / 1024.0f, 9.0f / 1024.0f, 37.0f / 1024.0f, 0.5f);
	spr_boss_bullet = SheetSprite(spriteSheetSpace, 858.0f / 1024.0f, 230.0f / 1024.0f, 9.0f / 1024.0f, 54.0f / 1024.0f, 0.8f);
	spr_enemy = SheetSprite(spriteSheetSpace, 120.0f / 1024.0f, 520.0f / 1024.0f, 104.0f / 1024.0f, 84.0f / 1024.0f, 0.8f);

	boss = LoadTexture(RESOURCE_FOLDER"boss.png");
	spr_boss = SheetSprite(boss, 0.0f / 59.0f, 0.0f / 79.0f, 59.0f / 59.0f, 79.0f / 79.0f, 2.0f);

	//Load Audio
	Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);
	menuMusic = Mix_LoadMUS("menu.mp3");
	gameMusic = Mix_LoadMUS("game.mp3");
	shootingSound = Mix_LoadWAV("shoot.wav");

	//Load Text Texture
	text = LoadTexture(RESOURCE_FOLDER"font1.png");
	menuText = LoadTexture(RESOURCE_FOLDER"font2.png");
}

//Main menu renderer
//Title will move with animation
void MainMenuRender(Matrix& model, Matrix& view, ShaderProgram& program, float animationValue)
{
	model.Identity();
	model.Translate(easeOutElastic(-4.1f, -2.1f, animationValue), easeOutElastic(4.2f, 1.5f, animationValue), 0.0f);
	program.SetModelviewMatrix(model * view);
	DrawText(&program, menuText, "Space Shooters", 0.6, -.25);

	model.Identity();
	model.Translate(easeOutElastic(0.1f, -2.1f, animationValue), easeOutElastic(4.2f, 1.5f, animationValue), 0.0f);
	program.SetModelviewMatrix(model * view);
	DrawText(&program, menuText, "Space Shooters", 0.6, -.25);

	model.Identity();
	model.Translate(-2.0f, 0.0f, 0);
	program.SetModelviewMatrix(model * view);
	DrawText(&program, menuText, "Press F1(single) or F2(multi) to start the game", .2, -.1);

	model.Identity();
	model.Translate(-1.1f, -1.0f, 0);
	program.SetModelviewMatrix(model * view);
	DrawText(&program, menuText, "Press ESC to exit the game", .2, -.1);

	view.Identity();
	program.SetModelviewMatrix(model * view);
}

//Game over renderer
void GameOverRender(Matrix& model, Matrix& view, ShaderProgram& program)
{
	model.Identity();
	model.Translate(-1.2, 0.5, 0);
	program.SetModelviewMatrix(model * view);
	DrawText(&program, menuText, "GAME OVER", 0.6, -.25);

	model.Identity();
	model.Translate(-1.4, -0.5, 0);
	program.SetModelviewMatrix(model * view);
	DrawText(&program, menuText, "Press Enter to return to main menu", .2, -.1);

	model.Identity();
	model.Translate(-1.1, -1.0, 0);
	program.SetModelviewMatrix(model * view);
	DrawText(&program, menuText, "Press ESC to exit the game", .2, -.1);

	view.Identity();
	program.SetModelviewMatrix(model * view);
}

//Renderer for game
void MainGameRender(Matrix& model, Matrix& view, ShaderProgram& program) {

	//Call render function of all Entity
	theBoss.Render(model, view, program);

	player.Render(model, view, program);
	if (numPlayer == 2) {
		player2.Render(model, view, program);
	}
	for (Entity star : stars) {
		star.Render(model, view, program);
	}

	for (Entity bullet : playerBullets) {
		bullet.Render(model, view, program);
	}

	for (Entity bossBullet : bossBullets) {
		bossBullet.Render(model, view, program);
	}

	for (Enemy enemy : enemies) {
		enemy.Render(model, view, program);
	}

	//Show current level
	if (showLevel) {
		model.Identity();
		model.Translate(-1.0f, 1.0f, 0.0f);
		program.SetModelviewMatrix(model * view);
		DrawText(&program, text, "Level " + std::to_string(currentLevel), 0.5f, -.15f);
	}

	//Show FPS, timer and number of bullets
	if (showStatistics) {
		model.Identity();
		model.Translate(-7.0f, 3.3f, 0.0f);
		program.SetModelviewMatrix(model * view);
		DrawText(&program, text, "Number of Bullet: " + std::to_string(noOfBullet), 0.3f, -.15f);

		model.Identity();
		model.Translate(-7.0f, 3.9f, 0.0f);
		program.SetModelviewMatrix(model * view);
		DrawText(&program, text, "FPS: " + std::to_string(60), 0.3f, -.15f);

		model.Identity();
		model.Translate(-7.0f, 3.6f, 0.0f);
		program.SetModelviewMatrix(model * view);
		DrawText(&program, text, "Timer: " + std::to_string(gameSeconds), 0.3f, -.15f);

		model.Identity();
		model.Translate(-7.0f, 3.0f, 0.0f);
		program.SetModelviewMatrix(modelMatrix * viewMatrix);	
		DrawText(&program, text, "Current Round: " + std::to_string(currentLevel), 0.3f, -.15f);
	}

	//Shake screen
	if(screenShake){
		view.Identity();
		view.Translate(cos(screenShakeValue * screenShakeSpeed) * screenShakeIntensity, 0.0f, 0.0f);
		program.SetModelviewMatrix(model * view);
	}

	else {
		view.Identity();
		program.SetModelviewMatrix(model * view);
	}
}

//Main Update function of the game
void Update(float elapsed) {
	//2 Seconds after player dead move to game over
	if (playerDeadTimer % 120 == 0) {
		state = STATE_GAME_OVER;
	}

	//2 seconds after boss dead move to game over
	if (bossDeadTimer % 120 == 0) {
		state = STATE_GAME_OVER;
	}

	//Increase timer every frame to keep track of time
	timer++;
	//Frame rate is 60 so every seconds = timer / 60
	gameSeconds = timer / 60;

	//At the first frame spawn the players
	if (timer == 1) {
		if(numPlayer == 1)
			player = Player(Vector3(0.0f, 0.0f, 0.0f), spr_player, ENTITY_PLAYER, false, true, 400);
		else if (numPlayer == 2) {
			player = Player(Vector3(1.0f, 0.0f, 0.0f), spr_player, ENTITY_PLAYER, false, true, 400);
			player2 = Player(Vector3(-1.0f, 0.0f, 0.0f), spr_player_2, ENTITY_PLAYER, false, true, 400);
		}
	}

	//start player dead timer when player(s) are destoryed
	if (numPlayer == 1) {
		if (!player.active) {
			playerDeadTimer++;
		}
	}

	else if (numPlayer == 2) {
		if (!player.active && !player2.active) {
			playerDeadTimer++;
		}
	}

	//Start boos dead timer when boss is destroyed
	if (bossSpawned && !theBoss.active) {
		bossDeadTimer++;
	}

	//do stuff level based on timer
	if (gameSeconds == 2) {
		showLevel = false;
	}

	//stop spawning at second 20
	else if (gameSeconds == 20) {
		spawnEnemy = false;
	}

	//move to level 2 at second 23
	else if (gameSeconds == 23) {
		currentLevel = 2;
		player.ammo = 400;
		player2.ammo = 400;
		spawnEnemy = true;
		showLevel = true;
	}

	else if (gameSeconds == 25) {
		showLevel = false;
	}

	//stop spawning at second 45
	else if (gameSeconds == 45) {
		spawnEnemy = false;
	}

	//move to level 3 at second 48
	else if (gameSeconds == 48) {
		currentLevel = 3;
		player.ammo = 400;
		player2.ammo = 400;
		spawnEnemy = true;
		showLevel = true;
		theBoss = Boss(Vector3(0.0f, 4.5f, 0.0f), spr_boss, ENTITY_BOSS, false, true, 200);
		bossSpawned = true;
	}

	else if (gameSeconds == 50) {
		showLevel = false;
	}

	//Shake screen for 0.25s
	if (screenShake) {
		shakeTimer++;
		screenShakeValue += elapsed;
		if (shakeTimer % 30 == 0) {
			screenShake = false;
			shakeTimer = 0;
		}
	}

	//processInput(player, elapsed);

	//Spawn enemy every 2 seconds
	if (timer % 120 == 0 && spawnEnemy) {
		enemies.push_back(Enemy(Vector3(5.0f, 5.0f, 0.0f), spr_enemy, ENTITY_ENEMY, false, true, 10, 2));
		enemies.push_back(Enemy(Vector3(-5.0f, 5.0f, 0.0f), spr_enemy, ENTITY_ENEMY, false, true, 10, 1));
	}

	//Update boss if its active in the scene and level is 3 and update bullet spawning timer
	if (currentLevel == 3 && theBoss.active == true) {
		theBoss.Update(elapsed);
		bossStraightBulletTimer++;
		bossDiagonalBulletTimer++;
	}

	//boss will shoot bullet for 4 seconds before stopping for 2 seconds
	if (spawnBullet) {
		bossSpawnBulletTimer++;
		if (bossSpawnBulletTimer % 240 == 0) {
			spawnBullet = false;
			bossSpawnBulletTimer = 1;
		}
	}

	else {
		bossNotSpawnBulletTimer++;
		if (bossNotSpawnBulletTimer % 120 == 0) {
			spawnBullet = true;
			bossNotSpawnBulletTimer = 1;
		}
	}

	//Boss shoot straight bullet every 1/3 seconds and diagonal one every 2/3 seconds
	if (spawnBullet && bossStraightBulletTimer % 20 == 0) {
		bossBullets.push_back(Entity(Vector3(theBoss.position.x - 0.4f, theBoss.position.y - 1.0f, theBoss.position.z), spr_boss_bullet, ENTITY_BOSS_BULLET, false, true));
		bossBullets.push_back(Entity(Vector3(theBoss.position.x + 0.4f, theBoss.position.y - 1.0f, theBoss.position.z), spr_boss_bullet, ENTITY_BOSS_BULLET, false, true));
	}

	if (spawnBullet && bossDiagonalBulletTimer % 40 == 0) {
		bossBullets.push_back(Entity(Vector3(theBoss.position.x - 1.0f, theBoss.position.y - 0.8f, theBoss.position.z), spr_boss_bullet, ENTITY_BOSS_BULLET_2, false, true));
		bossBullets.push_back(Entity(Vector3(theBoss.position.x + 1.0f, theBoss.position.y - 0.8f, theBoss.position.z), spr_boss_bullet, ENTITY_BOSS_BULLET_3, false, true));
	}

	//Spawn stars every frame
	starXPosition = -7.0f + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (7.0f + 7.0f)));
	stars.push_back(Entity(Vector3(starXPosition, 4.0f, 0.0f), spr_star, ENTITY_STAR, true, true));

	//Update player bullet every frame
	for (size_t i = 0; i < playerBullets.size(); i++) {
		playerBullets[i].Update(elapsed);
		if (playerBullets[i].active == false) {
			playerBullets.erase(playerBullets.begin() + i);
			noOfBullet--;
		}
	}

	//Update boos bullet every frame
	for (size_t i = 0; i < bossBullets.size(); i++) {
		bossBullets[i].Update(elapsed);
		if (bossBullets[i].active == false) {
			bossBullets.erase(bossBullets.begin() + i);
		}
	}

	//Update star every frame
	for (size_t i = 0; i < stars.size(); i++) {
		stars[i].Update(elapsed);
		if (stars[i].active == false) {
			stars.erase(stars.begin() + i);
		}
	}

	//Update enemies every frame
	for (size_t i = 0; i < enemies.size(); i++) {
		enemies[i].Update(elapsed);
		if (enemies[i].active == false) {
			enemies.erase(enemies.begin() + i);
		}
	}
}

int main(int argc, char *argv[])
{

	Setup();

	ShaderProgram program(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");

	float accumulator = 0.0f;

	SDL_Event event;
	done = false;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
		}

		ticks = (float)SDL_GetTicks() / 1000.0f;
		elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;

		elapsed += accumulator;
		if (elapsed < FIXED_TIMESTEP) {
			accumulator = elapsed;
			continue;
		}

		while (elapsed >= FIXED_TIMESTEP) {
			processInput(player, player2, elapsed);
			if (state == STATE_GAME_LEVEL) {
				Update(FIXED_TIMESTEP);
			}
			elapsed -= FIXED_TIMESTEP;
		}
		accumulator = elapsed;

		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(program.programID);
		program.SetModelviewMatrix(modelMatrix * viewMatrix);
		program.SetProjectionMatrix(projectionMatrix);
		
		//Render each scene based on state
		switch (state) {
		case(STATE_MAIN_MENU):
			animationTime += elapsed;

			animationValue = mapValue(animationTime, animationStart, animationEnd, 0.0, 1.0);

			MainMenuRender(modelMatrix, viewMatrix, program, animationValue);
			if (!playMenuMusic) {
				Mix_PlayMusic(menuMusic, -1);
				playGameMusic = false;
				playMenuMusic = true;
			}
			break;
		case(STATE_GAME_LEVEL):
			MainGameRender(modelMatrix, viewMatrix, program);
			if (!playGameMusic) {
				Mix_PlayMusic(gameMusic, -1);
				playGameMusic = true;
				playMenuMusic = false;
			}
			break;
		case(STATE_GAME_OVER):
			GameOverRender(modelMatrix, viewMatrix, program);
			Mix_PauseMusic();
			playGameMusic = true;
			playMenuMusic = false;
			break;
		}

		SDL_GL_SwapWindow(displayWindow);
	}

	//Free music
	Mix_FreeMusic(gameMusic);
	Mix_FreeMusic(menuMusic);
	Mix_FreeChunk(shootingSound);

	SDL_Quit();
	return 0;
}
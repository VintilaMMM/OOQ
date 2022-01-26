#include "game.h"

#include "config.h"

#include <algorithm>
#include <limits>
#include <cmath>
#include <utility>
#include <fstream>

MapManager::MapManager(GameManager *parent) :
	parent(parent),
	renderer(parent->getRenderer()),
	texture_manager(renderer->getTextureManager())
{
	// load available maps
	std::ifstream maps_file("data/maps.txt");

	int id;
	std::filesystem::path path;

	while (maps_file >> id >> path) {
		if (id >= maps.size())
			maps.resize(id + 1);

		maps[id] = path;
	}

	// debug
	loadMap(1);
}

void MapManager::loadMap(int map)
{
	// don't reload maps?
	//if (map == current_map)
	//	return;

	std::ifstream data(maps[map]);

	// update current map
	current_map = map;

	// clear old data
	resizeMapStorage(-1, -1, true);

	// read default spawn coords
	data >> spawn_x >> spawn_y;

	int pos_x, pos_y;
	std::filesystem::path path;

	while (data >> pos_x >> pos_y >> path) {
		// expand map storage if needed
		resizeMapStorage(pos_x, pos_y);

		if (path.extension() == ".png") {
			// load tile
			// TODO: implement layers
			int coll, layer;
			data >> coll >> layer;

			tile[pos_x][pos_y] = texture_manager->loadTexture(path);
			collision[pos_x][pos_y] = coll;
		} else if (path.extension() == ".txt") {
			// load object
			// TODO: implement
		}
	}
}

void MapManager::getSpawn(int *x, int *y)
{
	*x = spawn_x;
	*y = spawn_y;
}

int MapManager::getCollision(int pos_x, int pos_y)
{
	if (pos_x < 0 or pos_y < 0 or pos_x >= collision.size() or pos_y >=
	    collision[pos_x].size())
		return 1; // default collision for OOB

	return collision[pos_x][pos_y];
}

void MapManager::render()
{
	for (int i = 0; i < tile.size(); ++i)
		for (int j = 0; j < tile[i].size(); ++j)
			renderer->addRenderItem(tile[i][j], i * TILE_SIZE, j * TILE_SIZE, false, false, 0);
}

void MapManager::resizeMapStorage(int x, int y, bool absolute)
{
	// resize so that tile[x][y] is a valid position
	++x; ++y;
	int size_x, size_y;

	if (absolute) {
		size_x = x;
		size_y = y;
	} else {
		size_x = tile.size() > x ? tile.size() : x;

		if (not tile.empty())
			size_y = tile[0].size() > y ? tile[0].size() : y;
		else
			size_y = y;
	}

	tile.resize(size_x);
	collision.resize(size_x);

	for (int i = 0; i < size_x; ++i) {
		tile[i].resize(size_y, texture_manager->getMissingTexture());
		collision[i].resize(size_y, 1);
	}
}

GameObject::GameObject(GameManager *parent) :
	renderer(parent->getRenderer()),
	input_handler(parent->getManager()->getInputHandler()),
	map_manager(parent->getMapManager()),
	object_walker(nullptr),
	current_frame(0),
	loop_frame(0),
	end_frame(0),
	stop_frame(0),
	dir(DOWN),
	camera_center(false),
	size_x(1),
	size_y(1)
{
	// default position off screen
	setMapPos(-1, -1, false);

	// initialize textures with missing texture
	TextureManager *texture_manager = renderer->getTextureManager();

	up.resize(1);
	down.resize(1);
	side.resize(1);

	up[0] = texture_manager->getMissingTexture();
	down[0] = texture_manager->getMissingTexture();
	side[0] = texture_manager->getMissingTexture();
}

GameObject::~GameObject()
{
	if (object_walker)
		delete object_walker;
}

void GameObject::setScreenPos(int x, int y, bool anim)
{
	if (object_walker)
		object_walker->setDestination(x, y);
	else
		anim = false;

	if (not anim) {
		// skip animation, set to destination
		screen_x = x;
		screen_y = y;
	}
}

void GameObject::getScreenPos(int *x, int *y)
{
	*x = screen_x;
	*y = screen_y;
}

void GameObject::getCenter(int *x, int *y)
{
	*x = screen_x + TILE_SIZE * size_x / 2;
	*y = screen_y + TILE_SIZE * size_y / 2;
}

bool GameObject::isCameraCenter()
{
	return camera_center;
}

void GameObject::setMapPos(int x, int y, bool anim)
{
	map_x = x;
	map_y = y;

	// update ObjectWalker
	if (object_walker)
		object_walker->setDestination(map_x * TILE_SIZE, map_y * TILE_SIZE);
	else
		anim = false;

	if (not anim) {
		// if not animating, move now
		screen_x = map_x * TILE_SIZE;
		screen_y = map_y * TILE_SIZE;
	}
}

void GameObject::getMapPos(int *x, int *y)
{
	*x = map_x;
	*y = map_y;
}

void GameObject::getSize(int *x, int *y)
{
	*x = size_x;
	*y = size_y;
}

int GameObject::checkCollision(int offset_x, int offset_y)
{
	int tmp_x = map_x + offset_x;
	int tmp_y = map_y + offset_y;

	int coll = 0;

	for (int x = 0; x < size_x; ++x)
		for (int y = 0; y < size_y; ++y)
			coll = std::max(
				coll, 
				map_manager->getCollision(tmp_x + x, tmp_y + y)
			);

	return coll;
}

void GameObject::render()
{
	switch (dir) {
	case UP:
		renderer->addRenderItem(up[current_frame], screen_x, screen_y, false, false, 1);
		break;
	case LEFT:
		renderer->addRenderItem(side[current_frame], screen_x, screen_y, false, false, 1);
		break;
	case DOWN:
		renderer->addRenderItem(down[current_frame], screen_x, screen_y, false, false, 1);
		break;
	case RIGHT:
		renderer->addRenderItem(side[current_frame], screen_x, screen_y, true, false, 1);
		break;
	}
}

void GameObject::runTick(uint64_t delta)
{
	// run ObjectWalker
	if (object_walker)
		object_walker->runTick(delta);
}

void GameObject::_setScreenPos(int x, int y)
{
	screen_x = x;
	screen_y = y;
}

void GameObject::advanceFrame(DIR dir)
{
	this->dir = dir;
	if (++current_frame > end_frame) {
		current_frame = loop_frame;
	}
}

void GameObject::stopFrame(DIR dir)
{
	//this->dir = dir;
	//current_frame = current_frame != stop_frame ? stop_frame : 0;
	if (current_frame != stop_frame and current_frame != 0)
		current_frame = stop_frame;
	else if (current_frame != 0)
		current_frame = 0;
}

ObjectWalker::ObjectWalker(GameObject *parent) :
	parent(parent)
{
	/* 
	 * useless, since constructor will set correct values
	 * when available
	 */
	//parent->getScreenPos(&dest_x, &dest_y);
}

void ObjectWalker::setDestination(int x, int y)
{
	dest_x = x;
	dest_y = y;

	// respond instantly to new destination
	movement_deadline = 0;
	animation_deadline = 0;
}

/*
void ObjectWalker::cancel()
{
	parent->getScreenPos(&dest_x, &dest_y);
}
*/

void ObjectWalker::runTick(uint64_t delta)
{
	tick += delta;

	if (movement_deadline < tick) {
		// temporary screen coords
		int tmp_x, tmp_y;
		// invert move direction
		int sgn_x, sgn_y;

		// get current position
		parent->getScreenPos(&tmp_x, &tmp_y);
		sgn_x = sgn(dest_x - tmp_x);
		sgn_y = sgn(dest_y - tmp_y);

		// move object
		tmp_x += sgn_x ;
		tmp_y += sgn_y;

		// check overshoot
		if (sgn_x != sgn(dest_x - tmp_x)) {
			tmp_x = dest_x;
			sgn_x = 0;
		}

		if (sgn_y != sgn(dest_y - tmp_y)) {
			tmp_y = dest_y;
			sgn_y = 0;
		}

		// get movement direction
		DIR dir = UP;
		if (std::abs(dest_x - tmp_x) > std::abs(dest_y - tmp_y)) {
			if (sgn_x >= 0)
				dir = LEFT;
			else
				dir = RIGHT;
		} else {
			if (sgn_y >= 0)
				dir = DOWN;
			else
				dir = UP;
		}

		// send screen position to object
		parent->_setScreenPos(tmp_x, tmp_y);

		movement_deadline = tick + SPEED;

		// send animation data to object
		if (animation_deadline < tick) {
			if (sgn_x == 0 and sgn_y == 0)
				parent->stopFrame(dir);
			else
				parent->advanceFrame(dir);

			animation_deadline = tick + FRAME_TIME;
		}
	}
}

Player::Player(GameManager *parent, int type) :
	GameObject(parent),
	type(type)
{
	// construct ObjectWalker
	object_walker = new ObjectWalker(this);

	// make camera center
	camera_center = true;

	// load spawn location
	int tmp_x, tmp_y;
	map_manager->getSpawn(&tmp_x, &tmp_y);
	setMapPos(tmp_x, tmp_y, false);

	// load textures
	// TODO: load male/female based on choice
	// TODO: massively improve texture loading logic

	TextureManager *texture_manager = renderer->getTextureManager();

	/*
	 * keep all frames in order
	 * even if duplicated
	 * pointers don't take much memory
	 * and it simplifies code massively
	 */

	// set ObjectWalker frames
	loop_frame = 1;
	end_frame = 4;
	stop_frame = 5;

	up.resize(stop_frame + 1);
	down.resize(stop_frame + 1);
	side.resize(stop_frame + 1);

	switch (type) {
	case 0:
		up[0] = texture_manager->loadTexture("data/sprite/mc_male/u.png");
		up[1] = texture_manager->loadTexture("data/sprite/mc_male/u1.png");
		up[2] = texture_manager->loadTexture("data/sprite/mc_male/u.png");
		up[3] = texture_manager->loadTexture("data/sprite/mc_male/u2.png");
		up[4] = texture_manager->loadTexture("data/sprite/mc_male/u.png");
		up[5] = texture_manager->loadTexture("data/sprite/mc_male/u.png");

		down[0] = texture_manager->loadTexture("data/sprite/mc_male/d.png");
		down[1] = texture_manager->loadTexture("data/sprite/mc_male/d1.png");
		down[2] = texture_manager->loadTexture("data/sprite/mc_male/d.png");
		down[3] = texture_manager->loadTexture("data/sprite/mc_male/d2.png");
		down[4] = texture_manager->loadTexture("data/sprite/mc_male/d.png");
		down[5] = texture_manager->loadTexture("data/sprite/mc_male/d.png");

		side[0] = texture_manager->loadTexture("data/sprite/mc_male/s.png");
		side[1] = texture_manager->loadTexture("data/sprite/mc_male/s1.png");
		side[2] = texture_manager->loadTexture("data/sprite/mc_male/s.png");
		side[3] = texture_manager->loadTexture("data/sprite/mc_male/s2.png");
		side[4] = texture_manager->loadTexture("data/sprite/mc_male/s.png");
		side[5] = texture_manager->loadTexture("data/sprite/mc_male/s3.png");

		break;

	case 1:
		up[0] = texture_manager->loadTexture("data/sprite/mc_female/u.png");
		up[1] = texture_manager->loadTexture("data/sprite/mc_female/u1.png");
		up[2] = texture_manager->loadTexture("data/sprite/mc_female/u.png");
		up[3] = texture_manager->loadTexture("data/sprite/mc_female/u2.png");
		up[4] = texture_manager->loadTexture("data/sprite/mc_female/u.png");
		up[5] = texture_manager->loadTexture("data/sprite/mc_female/u.png");

		down[0] = texture_manager->loadTexture("data/sprite/mc_female/d.png");
		down[1] = texture_manager->loadTexture("data/sprite/mc_female/d1.png");
		down[2] = texture_manager->loadTexture("data/sprite/mc_female/d.png");
		down[3] = texture_manager->loadTexture("data/sprite/mc_female/d2.png");
		down[4] = texture_manager->loadTexture("data/sprite/mc_female/d.png");
		down[5] = texture_manager->loadTexture("data/sprite/mc_female/d.png");

		side[0] = texture_manager->loadTexture("data/sprite/mc_female/s.png");
		side[1] = texture_manager->loadTexture("data/sprite/mc_female/s1.png");
		side[2] = texture_manager->loadTexture("data/sprite/mc_female/s.png");
		side[3] = texture_manager->loadTexture("data/sprite/mc_female/s2.png");
		side[4] = texture_manager->loadTexture("data/sprite/mc_female/s.png");
		side[5] = texture_manager->loadTexture("data/sprite/mc_female/s3.png");

		break;
	}
}

void Player::runTick(uint64_t delta)
{
	// run base class tick
	GameObject::runTick(delta);

	// get input
	if (current_frame == 0 or 
	    current_frame == stop_frame) {
		if (((input_handler->isPlayer(UP) and type == 0) or 
		    (input_handler->isPlayer2(UP) and type == 1)) and
		    checkCollision(0, -1) < 1)
			setMapPos(map_x, map_y - 1);

		if (((input_handler->isPlayer(RIGHT) and type == 0) or
		    (input_handler->isPlayer2(RIGHT) and type == 1)) and
		    checkCollision(1, 0) < 1)
			setMapPos(map_x + 1, map_y);

		if (((input_handler->isPlayer(DOWN) and type == 0) or
		    (input_handler->isPlayer2(DOWN) and type == 1)) and
		    checkCollision(0, 1) < 1)
			setMapPos(map_x, map_y + 1);

		if (((input_handler->isPlayer(LEFT) and type == 0) or
		    (input_handler->isPlayer2(LEFT) and type == 1)) and
		    checkCollision(-1, 0) < 1)
			setMapPos(map_x - 1, map_y);
	}
}

GameManager::GameManager(Manager *parent) :
	parent(parent),
	renderer(parent->getRenderer()),
	map_manager(this)
{
	// set renderer size
	// aspect ratio is 4:3 for classy feel
	static const int multiplier = 5;
	renderer->setSize(4 * multiplier * TILE_SIZE, 3 * multiplier * TILE_SIZE);

	// player should always be first object
	objects.push_back(new Player(this, 0));
	//objects.push_back(new Player(this, 1));

	// TODO: load objects from file
}

GameManager::~GameManager()
{
	for (auto obj : objects)
		delete obj;
}

Manager *GameManager::getManager()
{
	return parent;
}

Renderer *GameManager::getRenderer()
{
	return renderer;
}

MapManager *GameManager::getMapManager()
{
	return &map_manager;
}

Player *GameManager::getPlayer()
{
	return static_cast<Player *>(objects.front());
}

void GameManager::setPaused(bool paused)
{
	this->paused = paused;
}

bool GameManager::getPaused()
{
	return paused;
}

// the following functions return bogus data for now
int GameManager::getCollected()
{
	return 2;
}

int GameManager::getRemaining()
{
	return 6 - 2;
}

uint64_t GameManager::getPlaytime()
{
	// almost an hour
	return 30000000;
}

void GameManager::runTick(uint64_t delta)
{
	// render map tiles
	map_manager.render();

	// calculate camera center
	int camera_count = 0;
	int camera_x = 0;
	int camera_y = 0;
	// calculate map size to include everything
	int min_x = std::numeric_limits<int>::max();
	int min_y = std::numeric_limits<int>::max();
	int max_x = std::numeric_limits<int>::min();
	int max_y = std::numeric_limits<int>::min();

	for (auto obj : objects) {
		// run object tick
		if (not paused)
			obj->runTick(delta);
		obj->render();

		// camera calculations
		if (obj->isCameraCenter()) {
			int tmp_x, tmp_y;

			obj->getCenter(&tmp_x, &tmp_y);

			camera_count++;
			camera_x += tmp_x;
			camera_y += tmp_y;

			min_x = std::min(min_x, tmp_x);
			min_y = std::min(min_y, tmp_y);
			max_x = std::max(max_x, tmp_x);
			max_y = std::max(max_y, tmp_y);
		}
	}

	if (camera_count > 0) {
		camera_x /= camera_count;
		camera_y /= camera_count;
	}

	int size_x = std::abs(max_x - min_x);
	int size_y = std::abs(max_y - min_y);
	int multiplier = 5;

	while (4 * multiplier * TILE_SIZE < size_x and 3 * multiplier * TILE_SIZE < size_y) 
		++multiplier;

	renderer->setCenter(camera_x, camera_y);
	renderer->setSize(4 * multiplier * TILE_SIZE, 3 * multiplier * TILE_SIZE);
}

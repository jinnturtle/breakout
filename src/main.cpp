#include <iostream>
using std::cout;
using std::cin;
using std::cerr;
using std::endl;
#include <vector>
using std::vector;
#include <string>
using std::string;
using std::to_string;
#include <memory>
using std::shared_ptr;
#include <fstream>
using std::ifstream;
using std::ofstream;

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include "utils.h"
#include "Timer.h"
#include "Paddle.h"
#include "Ball.h"
#include "Brick.h"
#include "Text_Object.h"

const int win_w = 800;
const int win_h = 640;

int init(); //initialise SDL
int create_main_win(SDL_Window*& _win, SDL_Surface*& _srf,
                    const int _w, const int _h);
int create_win_renderer(SDL_Window* _win, SDL_Renderer*& _ren);
void close(SDL_Window*& _win, SDL_Renderer*& _ren);
SDL_Surface* load_surface(const string& _path);
SDL_Texture* load_texture(const string& _path, SDL_Renderer* _ren);
vector<shared_ptr<SDL_Texture>> load_textures(SDL_Renderer* _ren);
void run_game(SDL_Renderer* _ren, const int _win_w, const int _win_h,
              vector<shared_ptr<SDL_Texture>>* _texs);
void save_score(int _score, string _name = "player");
void outro(SDL_Renderer* _ren, const int _win_w, const int _win_h);
int check_loss(SDL_Rect* _r, const int _max_h);
void load_endgame_screen(const string& _end_txt, SDL_Renderer* _ren);
void make_bricks(vector<shared_ptr<Brick>>& _bricks,
                 shared_ptr<SDL_Texture> _tex);
char coll_detect(SDL_Rect* _a, SDL_Rect* _b); //did _a hit _b?

int main(int argc, char* args[])
{
	if(init() != 0) {
		cerr << "the application will now exit.\n";
		return 1;
	}
	
	SDL_Window* win_main = NULL;
	SDL_Surface* srf_main = NULL;
	SDL_Renderer* ren_main = NULL;
	
	if(create_main_win(win_main, srf_main, win_w, win_h) != 0) {
		close(win_main, ren_main);
		return 1;
	}
	
	if(create_win_renderer(win_main, ren_main) != 0) {
		close(win_main, ren_main);
		return 1;
	}
	
	vector<shared_ptr<SDL_Texture>> texs = load_textures(ren_main);
	if(texs.size() == 0) {
		return 1;
	}
	
	run_game(ren_main, win_w, win_h, &texs);
	
	outro(ren_main, win_w, win_h);
	
	close(win_main, ren_main);
	return 0;
}

int init()
{
	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
		cerr << "ERROR: FATAL - could not initialise SDL!\n";
		cerr << "SDL error = " << SDL_GetError() << endl;
		return 1;
	}
	
	int sdl_image_flags = IMG_INIT_PNG;
	if(!(IMG_Init(sdl_image_flags) & sdl_image_flags)) {
		cerr << "ERROR: FATAL - could not initialise SDL_image!\n";
		cerr << "SDL_image error = " << IMG_GetError() << endl;
		return 1;
	}

	if(TTF_Init() == -1) {
		cerr << "ERROR: FATAL - could not initialise SDL_ttf!\n";
		cerr << "SDL_ttf error = " << TTF_GetError() << endl;
		return 1;
	}
	
	return 0;
}

int create_main_win(SDL_Window*& _win, SDL_Surface*& _srf,
										const int _w, const int _h)
{
	_win = SDL_CreateWindow("breakout42",
	                        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
	                        _w, _h,
	                         SDL_WINDOW_SHOWN);
	if(_win == NULL) {
		cerr << "ERROR: FATAL - SDL window could not be created!\n";
		cerr << "SDL error = " << SDL_GetError() << endl;
		return 1;
	}
	
	_srf = SDL_GetWindowSurface(_win);
	
	return 0;
}

int create_win_renderer(SDL_Window* _win, SDL_Renderer*& _ren)
{
	_ren = SDL_CreateRenderer(_win, -1, SDL_RENDERER_ACCELERATED);
	
	if(_ren == NULL) {
		cerr << "ERROR: Could not create renderer!\n";
		cerr << "SDL error = " << SDL_GetError() << endl;
		return 1;
	}
	
	//initialise renderer colour
	SDL_SetRenderDrawColor(_ren, 0xff, 0xff, 0xff, 0xff);
	
	return 0;
}

void close(SDL_Window*& _win, SDL_Renderer*& _ren)
{
	if(_win != NULL) {SDL_DestroyWindow(_win);}
	_win = NULL;
	
	if(_ren != NULL) {SDL_DestroyRenderer(_ren);}
	_ren = NULL;
	
	IMG_Quit();
	SDL_Quit();
	TTF_Quit();
}

SDL_Surface* load_surface(const string& _path)
{
	SDL_Surface* loaded = IMG_Load(_path.c_str());
	if(loaded == NULL){
		cerr << "ERROR: Could not load picture!\n";
		cerr << "file path: " << _path << endl;
		cerr << "SDL error = " << IMG_GetError() << endl;
		return NULL;
	}
	
	return loaded;
}

SDL_Texture* load_texture(const string& _path, SDL_Renderer* _ren)
{
	SDL_Texture* tex = NULL;
	
	SDL_Surface* tmp_surf = load_surface(_path);
	if(tmp_surf == NULL) {
		return NULL;
	}
	
	tex = SDL_CreateTextureFromSurface(_ren, tmp_surf);
	if(tex == NULL) {
		cerr << "ERROR: Could not create texture!\n";
		cerr << "file path: " << _path << endl;
		cerr << "SDL error = " << SDL_GetError() << endl;
		return NULL;
	}
	
	SDL_FreeSurface(tmp_surf);
	
	return tex;
}

void run_game(SDL_Renderer* _ren, const int _win_w, const int _win_h,
              vector<shared_ptr<SDL_Texture>>* _texs)
{
	//init game
	Timer loop_timer;
	const short tgt_fps = 60;
	Uint32 tgt_frame_len = 1000 / tgt_fps;
	Uint32 frame_len = 0;
	bool show_fps = true;

	string def_font_path = "assets/fonts/DejaVuSansMono.ttf";

	string endgame_txt = "";
	int score0 = 0;
	unsigned short lives0 = 3;

	Paddle paddle0(SDL_Rect{300, 600}, 7);
	
	Ball ball(SDL_Rect{400, 300}, 9, 45);
	vector<shared_ptr<Brick>> bricks;
	make_bricks(bricks, (*_texs)[2]);
	
	paddle0.assign_texture((*_texs)[0]);
	ball.assign_texture((*_texs)[1]);

	Text_Object lives0_txt(5, def_font_path, 20,
	                       SDL_Colour{0x30, 0x80, 0xf0, 0x00},
	                       _ren,
	                       SDL_Rect{700, 20});
	Text_Object score0_txt(5, def_font_path, 20,
	                       SDL_Colour{0x30, 0x80, 0xf0, 0x00},
	                       _ren,
	                       SDL_Rect{20, 20});
	Text_Object fps_txt(5, def_font_path, 20,
	                    SDL_Colour{0x30, 0x80, 0xf0, 0x00},
	                    _ren,
	                    SDL_Rect{5, 5});

	lives0_txt.set_text_ln(0, "LIVES: " + to_string(lives0));
	score0_txt.set_text_ln(0, "SCORE: " + to_string(score0));
	fps_txt.set_text_ln(0, "FPS: ");

	lives0_txt.redraw();
	score0_txt.redraw();
	fps_txt.redraw();

	//main loop
	bool pause = false;
	bool flag_quit = false;
	while(flag_quit == false) {
		loop_timer.set_start(SDL_GetTicks());
		
		//input and update phase
		SDL_Event event;
		const Uint8* key_states = SDL_GetKeyboardState(NULL);
		//key down/up check
		while(SDL_PollEvent(&event) != 0) {
			if(event.type == SDL_KEYDOWN) {
				if(key_states[SDL_SCANCODE_F]) {
					if(show_fps) {show_fps = false;}
					else {show_fps = true;}
				}
				if(key_states[SDL_SCANCODE_P]) {
					if(pause) {pause = false;}
					else {pause = true;}
				}
			}
			else if(event.type == SDL_QUIT) {
				flag_quit = true;
			}
		}

		if(pause) {SDL_Delay(30); continue;}

		//key pressed check
		//TODO see if can be moved to loop above (maybe catch another type evnt)
		if(key_states[SDL_SCANCODE_LEFT]) {
			paddle0.move_l();
		}
		else if(key_states[SDL_SCANCODE_RIGHT]) {
			paddle0.move_r();
		}

		int old_score0 = score0;

		unsigned hit_brick_id = -1;
		SDL_Rect* hit_brick_rect = NULL;
		for(unsigned i = 0; i < bricks.size(); ++i) {
			if(coll_detect(ball.get_rect(), bricks[i]->get_rect()) != 0) {
				hit_brick_id = i;
				cerr << "hit brick: " << hit_brick_id << endl;
				hit_brick_rect = bricks[i]->get_rect();

			cerr << "hbr x/y/w/h: "
				 << hit_brick_rect->x << "/"
				 << hit_brick_rect->y << "/"
				 << hit_brick_rect->w << "/"
				 << hit_brick_rect->h << endl;

				break;
			}
		}

		ball.update(win_w, win_h, hit_brick_rect, paddle0.get_rect(), score0);
		//if brick id is set - collision happened. Destroy that brick.
		if(hit_brick_id <= bricks.size()) {
			bricks[hit_brick_id] = bricks.back();
			bricks.pop_back();
		}

		if(check_loss(ball.get_rect(), win_h)) {
			//if ball lost, decrement lives, end game if lives == 0
			if(--lives0 == 0) {
				flag_quit = true;
				endgame_txt = "GAME OVER!";
				cout << endgame_txt << "\nTRY AGAIN ;)\n";
			}
			cerr << "lives: " << lives0 << endl;
			lives0_txt.set_text_ln(0, "LIVES: " + to_string(lives0));
			lives0_txt.redraw();
			ball = Ball(SDL_Rect{400, 300}, 9, 45);
			ball.assign_texture((*_texs)[1]);
		}

		if(old_score0 != score0) {
			score0_txt.set_text_ln(0, "SCORE: " + to_string(score0));
			score0_txt.redraw();
		}

		//if all bricks are gone, make a new array when ball is close to paddle
		if(bricks.size() == 0) {
			ball = Ball(SDL_Rect{400, 300}, 9, 45);
			ball.assign_texture((*_texs)[1]);
			make_bricks(bricks, (*_texs)[2]);
		}

		if(show_fps) {
			if(frame_len > 0) {
				fps_txt.set_text_ln(0, "FPS: " + to_string(1000 / frame_len));
				fps_txt.redraw();
			}
			else {
				fps_txt.set_text_ln(0, "FPS: " + to_string(tgt_fps));
				fps_txt.redraw();
			}
		}

		//render phase
		SDL_SetRenderDrawColor(_ren, 0x00, 0x20, 0x20, 0xff);
		SDL_RenderClear(_ren);
		
		paddle0.render(_ren);
		ball.render(_ren);
		for(unsigned short i = 0; i < bricks.size(); ++i) {
			bricks[i]->render(_ren);
		}
		lives0_txt.render();
		score0_txt.render();
		if(show_fps) {fps_txt.render();}

		if(flag_quit == true) {
			cout << "final score: " << score0 << endl;
			load_endgame_screen(endgame_txt, _ren);
			SDL_RenderPresent(_ren);

			save_score(score0);

			SDL_Delay(1000); //wait a sec
			break;
		}
		
		SDL_RenderPresent(_ren);

		//limit framerate
		loop_timer.set_end(SDL_GetTicks());
		frame_len = loop_timer.get_duration();
		if(frame_len < tgt_frame_len) {
			Uint32 wait_len = tgt_frame_len - frame_len;
			if(wait_len > 0) {
				SDL_Delay(wait_len);
				frame_len += wait_len;
			}
		}
	}
}

void save_score(int _score, string _name) //default: _name = "player"
{
	bool new_hi = false;
	unsigned max_entries = 10;
	string sep = ";";
	string line;
	ifstream inp("data/scoreboard");	
	vector<string> lines;
	
	for(unsigned i = 0; !inp.eof() && i < max_entries; ++i) {
		getline(inp, line);
		if(inp.eof()) {break;}

		size_t pos = line.find(sep);
		if(pos == string::npos) {continue;}

		string name = line.substr(0, pos - 1);
		int score = stoi(line.substr(pos + 1));

		if(score <= _score && !new_hi) {
			lines.push_back(_name + sep + to_string(_score));
			new_hi = true;
			++i;
		}
		if(lines.size() < max_entries) {lines.push_back(line);}
	}

	inp.close();

	if(!new_hi && lines.size() < max_entries) {
		lines.push_back(_name + sep + to_string(_score));
	}

	ofstream out("data/scoreboard");
	for(unsigned i = 0; i < lines.size(); ++i) {
		out << lines[i] << endl;
	}

	out.close();

	if(new_hi) {cout << "New highscore, congratulations!\n";}
}

//TODO polish the animation closer to the end of the project
void outro(SDL_Renderer* _ren, const int _win_w, const int _win_h)
{
	vector<Text_Object*> text(2);
	string def_font_path = "assets/fonts/DejaVuSansMono.ttf";
	text[0] = new Text_Object(5, def_font_path, 36
	                         , SDL_Colour{0x80, 0x00, 0x00, 0x00}
	                         , _ren
	                         , SDL_Rect{200, 100});
	text[1] = new Text_Object(5, def_font_path, 18
	                         , SDL_Colour{0x80, 0x00, 0x00, 0x00}
	                         , _ren
	                         , SDL_Rect{200, 150});
	text[2] = new Text_Object(5, def_font_path, 18
	                         , SDL_Colour{0x80, 0x00, 0x00, 0x00}
	                         , _ren
	                         , SDL_Rect{200, 200});

	text[0]->set_text_ln(0, "GAME OVER");
	text[0]->redraw();
	text[1]->set_text_ln(0, "The Top 10 players so far!");
	text[1]->redraw();
	if(text[2]->load_file("data/scoreboard") < 0) {
		cerr << "WARNING: Could not read scoreboard data\n";
	}
	else {
		text[2]->redraw();
	}

	unsigned short linenum = 10;
	int lines_ay[linenum];
	int lines_by[linenum];
	float lines_dv_y[linenum];
	int start_y = _win_h / 2;
	for(unsigned i = 0; i < linenum; ++i) {
		lines_ay[i] = start_y;
		lines_by[i] = start_y;
		lines_dv_y[i] = (1 + i) * 2;
		start_y *= 1.1f;
	}

	bool flag_quit = false;
	while(!flag_quit) {
		SDL_Event event;
		while(SDL_PollEvent(&event) != 0) {
			if(event.type == SDL_QUIT) {
				flag_quit = true;
			}
		}

		SDL_SetRenderDrawColor(_ren, 0xff, 0xff, 0xff, 0xff);
		SDL_RenderClear(_ren);
		//SDL_RenderCopy(_ren, texs[srf_action_x], NULL, NULL);
		
		//draw primitives
		SDL_SetRenderDrawColor(_ren, 0xff, 0x00, 0x00, 0xff);
		for(unsigned i = 0; i < linenum; ++i) {
			SDL_RenderDrawLine(_ren, 0, lines_ay[i], _win_w, lines_by[i]);
			lines_ay[i] += lines_dv_y[i];
			lines_by[i] += lines_dv_y[i];
			lines_dv_y[i] *= 1.05f;
			if(lines_ay[i] > _win_h) {
				lines_ay[i] = _win_h / 2;
				lines_by[i] = _win_h / 2;
				lines_dv_y[i] = 1;
			}
		}

		text[0]->render();
		text[1]->render();
		text[2]->render();
		
		SDL_RenderPresent(_ren);
		SDL_Delay(30);
	}
}

vector<shared_ptr<SDL_Texture>> load_textures(SDL_Renderer* _ren)
{
	vector<shared_ptr<SDL_Texture>> texs;
	try {
		shared_ptr<SDL_Texture> shared_tex;
		SDL_Texture* tex;
		//0
		tex = load_texture("assets/gfx/paddle.png", _ren);
		shared_tex.reset(tex, SDL_DestroyTexture);
		texs.push_back(std::move(shared_tex));
		//1
		tex = load_texture("assets/gfx/ball.png", _ren);
		shared_tex.reset(tex, SDL_DestroyTexture);
		texs.push_back(shared_tex);
		//2
		tex = load_texture("assets/gfx/brick.png", _ren);
		shared_tex.reset(tex, SDL_DestroyTexture);
		texs.push_back(shared_tex);
	}
	catch(const char* err_msg) {
		cerr << err_msg << endl;
		cerr << "The program will now quit.\n";
		
		return vector<shared_ptr<SDL_Texture>>(0);
	}
	
	return texs;
}

int check_loss(SDL_Rect* _r, const int _max_y)
{
	if(_r->y > _max_y) {return 1;}

	return 0;
}

void load_endgame_screen(const string& _end_txt, SDL_Renderer* _ren)
{	
	Text_Object txt_o(5,
	                  "assets/fonts/DejaVuSansMono.ttf", 60,
	                  SDL_Colour{0x30, 0x80, 0xf0, 0x00},
	                  _ren,
	                  SDL_Rect{0,0});

	txt_o.set_text_ln(0, _end_txt);
	txt_o.redraw();

	txt_o.render_stretched();
}

void make_bricks(vector<shared_ptr<Brick>>& _bricks,
                 shared_ptr<SDL_Texture> _tex)
{
	unsigned short cols = 10, rows = 5;

	for(unsigned short i = 0; i < cols; ++i) {
		for(unsigned short j = 0; j < rows; ++j) {
			int x = 20 + (i * 60);
			int y = 20 + (j * 30);
			_bricks.push_back(shared_ptr<Brick> (new Brick(SDL_Rect{x, y})));
		}
	}

	for(unsigned short i = 0; i < _bricks.size(); ++i) {
		_bricks[i]->assign_texture(_tex);
	}
}

char coll_detect(SDL_Rect* _a, SDL_Rect* _b)
{
	if(_a->x > _b->x + _b->w
	|| _a->x + _a->w < _b->x
	|| _a->y > _b->y + _b->h
	|| _a->y + _a->h < _b->y
	) {
		return 0;
	}

	else {
		return 1;
	}
}

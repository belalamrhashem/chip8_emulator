#include <SDL3/SDL.h>
#include <stdio.h>  
#include <stdlib.h> 
#include <time.h>   
#include <string.h>
#include <stdint.h>

//Funcs
void init();
void cycle();
void draw(SDL_Texture* texture, SDL_Renderer* renderer);
int load_rom(char* filename);
unsigned short fetch();
void decode_execute(unsigned short opcode);

unsigned short opcode; //2 byte opcode
unsigned char memory[4096]; //4KB memory
unsigned char display[32][64];
unsigned char display_prev[32][64]; // for smoothness

//Registers
unsigned char V[16]; //General purpose registers 1-16 (16 is flag)
unsigned short I; //index register
unsigned short pc; //program counter

//Timers, count down to 0 at 60Hz
unsigned char delay_timer;
unsigned char sound_timer; //Buzzer sounds when this timer reaches 0
unsigned char drawFlag; //Indicates if we need to redraw the screen

//Stack, for jump instructions
unsigned short stack[16]; //Stores up to 16 PC values
unsigned short sp; //Stack top pointer

unsigned char key[16]; //Current state of keypad (4x4 grid, 1 is pressed and 0 is released)

//Adjustables
unsigned int speed_factor = 10;

//Fontset
unsigned char chip8_fontset[80] =
{
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};


int main(int argc, char* argv[]) {

	//Initialise and clear
	init();

	//Load ROM
	if (!load_rom("Pong.ch8")) return -1;

	// 1. Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		SDL_Log("SDL could not initialize! SDL error: %s", SDL_GetError());
		return 1;
	}

	// 2. Create the Window
	// In SDL3, we just pass Title, Width, Height, and Flags.
	SDL_Window* window = SDL_CreateWindow("Chip-8 Emulator", 1280, 640, 0);
	if (window == NULL) {
		SDL_Log("Window could not be created! SDL error: %s", SDL_GetError());
		return 1;
	}

	// 3. Create the Renderer
	// The second argument is the driver name (NULL lets SDL choose the best one).
	SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
	if (renderer == NULL) {
		SDL_Log("Renderer could not be created! SDL error: %s", SDL_GetError());
		return 1;
	}
	SDL_SetRenderLogicalPresentation(renderer, 64, 32, SDL_LOGICAL_PRESENTATION_STRETCH);


	// 4. Create the Texture
	SDL_Texture* texture = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_STREAMING,
		64, 32
	);

	// Main loop:
	char running = 1;
	SDL_Event event; //Holds input data

	while (running) {

		uint64_t start = SDL_GetTicks();

		//Input
		while (SDL_PollEvent(&event)) {

			//Quitting
			if (event.type == SDL_EVENT_QUIT) {
				running = 0; //If user requests to quit, break the loop.
			}

			//Keyboard input
			if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {

				//Pressed or released?
				char keyState;
				if (event.type == SDL_EVENT_KEY_DOWN) {
					keyState = 1;
				}
				else if (event.type == SDL_EVENT_KEY_UP) {
					keyState = 0;
				}

				//Which key was pressed/released?
				switch (event.key.key) {
				case SDLK_1:
					key[0x1] = keyState;
					break;
				case SDLK_2:
					key[0x2] = keyState;
					break;
				case SDLK_3:
					key[0x3] = keyState;
					break;
				case SDLK_4:
					key[0xC] = keyState;
					break;
				case SDLK_Q:
					key[0x4] = keyState;
					break;
				case SDLK_W:
					key[0x5] = keyState;
					break;
				case SDLK_E:
					key[0x6] = keyState;
					break;
				case SDLK_R:
					key[0xD] = keyState;
					break;
				case SDLK_A:
					key[0x7] = keyState;
					break;
				case SDLK_S:
					key[0x8] = keyState;
					break;
				case SDLK_D:
					key[0x9] = keyState;
					break;
				case SDLK_F:
					key[0xE] = keyState;
					break;
				case SDLK_Z:
					key[0xA] = keyState;
					break;
				case SDLK_X:
					key[0x0] = keyState;
					break;
				case SDLK_C:
					key[0xB] = keyState;
					break;
				case SDLK_V:
					key[0xF] = keyState;
					break;
				}
			}

		}
		//CPU
		for (int i = 0; i < speed_factor; i++)
		{
			cycle();
		}

		//Timers
		if (delay_timer > 0) {
			delay_timer--;
		}
		if (sound_timer > 0) {
			sound_timer--;
		}

		//Render
		if (drawFlag){
		draw(texture, renderer);
		drawFlag = 0;
		}

		memcpy(display_prev, display, sizeof(display));

		//FPS
		uint64_t end = SDL_GetTicks();
		float elapsedMS = (float)(end - start);

		if (elapsedMS < 16.667f) SDL_Delay((uint32_t)(16.667f - elapsedMS));
	}

	// Cleanup
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

void init() {
	//Initialise variables:
	I = 0;
	pc = 0x200;
	delay_timer = 0;
	sound_timer = 0;
	sp = 0;
	opcode = 0;

	//Clearing
	memset(display, 0, sizeof(display));
	memset(stack, 0, sizeof(stack));
	memset(V, 0, sizeof(V));
	memset(memory, 0, sizeof(memory));
	memset(key, 0, sizeof(key));
	

	//Initialise fontset
	
	for (int i = 0; i < sizeof(chip8_fontset); i++)
	{
		memory[0x50 + i] = chip8_fontset[i];
	}
}

void cycle() {
	//Combines fetch() and decode_execute()
	opcode = fetch();
	pc += 2;
	decode_execute(opcode);
}

void draw(SDL_Texture* texture, SDL_Renderer* renderer) {

	//Buffer
	uint32_t pixels[64*32];

	//Fill the buffer
	for (int row = 0; row < 32; row++)
	{
		for (int col = 0; col < 64; col++)
		{
			int index = row * 64 + col;
			if (display[row][col] == 1 || display_prev[row][col] == 1) {
				pixels[index] = 0xFFFFFFFF;
			}
			else {
				pixels[index] = 0xFF000000;
			}
		}
	}

	//Upload to texture
	SDL_UpdateTexture(texture, NULL, pixels, 256);

	//Clear screen (good practice)
	SDL_RenderClear(renderer);

	//Copy texture to renderer
	SDL_RenderTexture(renderer, texture, NULL, NULL);

	//Present to screen
	SDL_RenderPresent(renderer);
}

unsigned short fetch() {
	opcode = memory[pc] << 8 | memory[pc + 1];
	return opcode;
}

int load_rom(char* filename) {
	//Open file
	FILE* rom = fopen(filename, "rb");

	//If failed to open: (e.g. filename doesn't exist)
	if (rom == NULL) {
		printf("Failed to open ROM: %s\n", filename);
		return 0;
	}

	//Set cursor to end to get number of elements then return to start
	fseek(rom, 0, SEEK_END);
	long rom_size = ftell(rom);
	rewind(rom);

	//Does it fit?
	if (rom_size > (4096 - 512)) { //total RAM minus reserved RAM
		printf("Don't fit cuh");
		fclose(rom);
		return 0;
	}

	//Read the contents into the buffer (memory array)
	fread(&memory[0x200], 1, rom_size, rom);

	//Close and end
	fclose(rom);
	return 1;	
}

void decode_execute(unsigned short opcode) {
	unsigned short firstNib = (opcode & 0xF000) >> 12;
	unsigned char x = (opcode & 0x0F00) >> 8;
	unsigned char y = (opcode & 0x00F0) >> 4;
	unsigned char kk = (opcode & 0x00FF);
	unsigned char n = (opcode & 0x000F);

	switch (firstNib) {
	case 0x0:
		switch (kk) {
		case (0xE0):
			memset(display, 0, sizeof(display));
			break;

		case 0xEE:
			sp -= 1;
			pc = stack[sp];
			break;
		}
		break;

	case 0x1:
		pc = opcode & 0x0FFF;
		break;

	case 0x2:
		stack[sp] = pc;
		sp += 1;
		pc = opcode & 0x0FFF;
		break;

	case 0x3:
		if (V[x] == kk) {
			pc += 2;
		}
		break;

	case 0x4:
		if (V[x] != kk) {
			pc += 2;
		}
		break;

	case 0x5:
		if (V[x] == V[y]) {
			pc += 2;
		}
		break;

	case 0x6:
		V[x] = kk;
		break;

	case 0x7:
		V[x] += kk;
		break;

	case 0x8:
		switch (n) {
		case 0:
			V[x] = V[y];
			break;
		case 1:
			V[x] |= V[y];
			break;
		case 2:
			V[x] &= V[y];
			break;
		case 3:
			V[x] ^= V[y];
			break;
		case 4:
		{
			int temp = V[x] + V[y];
			if (temp > 255) {
				V[0xF] = 1;
			}
			else {
				V[0xF] = 0;
			}

			V[x] += V[y];
			break;
		}
		case 5:
		{
			int temp = V[x] - V[y];
			if (temp < 0) {
				V[0xF] = 0;
			}
			else {
				V[0xF] = 1;
			}

			V[x] -= V[y];
			break;
		}
		case 6:
			V[0xF] = V[x] & 0x1;
			V[x] >>= 1;
			break;
		case 7:
		{
			int temp = V[y] - V[x];
			if (temp < 0) {
				V[0xF] = 0;
			}
			else {
				V[0xF] = 1;
			}
			V[x] = V[y] - V[x];
			break;
		}
		case 0xE:
			V[0xF] = (V[x]>>7) & 0x1;
			V[x] <<= 1;
			break;
		}
		break;

	case 0x9:
		if (V[x] != V[y]) {
			pc += 2;
		}
		break;

	case 0xA:
		I = opcode & 0x0FFF;
		break;

	case 0xB:
		pc = V[0] + (opcode & 0x0FFF);
		break;

	case 0xC:
		V[x] = rand() & kk;
		break;

	case 0xD:
		V[0xF] = 0; //Reset collision flag
		drawFlag = 1; //Set draw flag

		//Fix wrapping issues
		unsigned short x_coord = V[x] % 64;
		unsigned short y_coord = V[y] % 32;

		unsigned short pixel_data;
		for (int row = 0; row < n; row++) {
			pixel_data = memory[I + row];
			for (int col = 0; col < 8; col++) {
				if ((pixel_data & (0x80 >> col)) != 0) { //This condition checks each bit by AND masking with 1000 0000

					if ((x_coord + col >= 64) || (y_coord + row >= 32)) {
						continue;
					}


					//First, check if collision occurs:
					if (display[y_coord + row][x_coord + col] == 1) {
						//Collision
						V[0xF] = 1;
					}

					display[y_coord + row][x_coord + col] ^= 1;

				}
			}
		}
		break;

	case 0xE:
		switch (kk) {
		case 0x9E:
			if (key[V[x]] == 1) {
				pc += 2;
			}
			break;
		case 0xA1:
			if (key[V[x]] == 0) {
				pc += 2;
			}
			break;
		}
		break;


	case 0xF:
		switch (kk) {
		case 0x07:
			V[x] = delay_timer;
			break;
		case 0x0A:
		{
			//pause execution until key pressed
			char keyFlag = 0;
			for (int i = 0; i < 16; i++)
			{
				if (key[i] == 1) {
					keyFlag = 1;
					V[x] = i;
					break;
				}
			}
			if (keyFlag == 0) {
				pc -= 2;
			}

			break;
		}
		case 0x15:
			delay_timer = V[x];
			break;
		case 0x18:
			sound_timer = V[x];
			break;
		case 0x1E:
			I += V[x];
			break;
		case 0x29:
			I = 0x50 + ((V[x] & 0x0F) * 5);
			break;
		case 0x33:
			memory[I] = V[x] / 100;
			memory[I + 1] = (V[x]/10) % 10;
			memory[I + 2] = V[x] % 10;
			break;
		case 0x55:
			for (int i = 0; i <= x; i++)
			{
				memory[I + i] = V[i];
			}
			break;
		case 0x65:
			for (int i = 0; i <= x; i++)
			{
				V[i] = memory[I + i];
			}
			break;
		}
		break;
	}

}
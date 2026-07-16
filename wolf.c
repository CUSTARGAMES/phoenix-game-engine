// wolf.c - Wolfenstein 3D Style Raycaster
// Compile: gcc wolf.c -o wolf.exe -lm -O2 -mwindows -static

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>
#include <time.h>

#define SCREEN_W 640
#define SCREEN_H 480
#define MAP_W 20
#define MAP_H 20
#define PI 3.14159265f
#define MOVE_SPEED 3.0f
#define ROT_SPEED 2.0f

// ========== MAP ==========
// 0 = empty, 1 = blue wall, 2 = green wall, 3 = red wall, 4 = yellow wall
int map[MAP_W][MAP_H] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,3,3,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,4,4,4,4,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

// ========== PLAYER ==========
typedef struct {
    float x, y;
    float dir_x, dir_y;
    float plane_x, plane_y;
    int health;
    int ammo;
    int shooting;
    float shoot_timer;
} Player;

Player player = {
    2.5f, 2.5f,
    1.0f, 0.0f,
    0.0f, 0.66f,
    100, 50, 0, 0
};

// ========== ENEMIES (Simple) ==========
typedef struct {
    float x, y;
    int health;
    int active;
    float last_shot;
} Enemy;

#define MAX_ENEMIES 20
Enemy enemies[MAX_ENEMIES];
int num_enemies = 0;

// ========== BULLETS ==========
typedef struct {
    float x, y;
    float dx, dy;
    float life;
    int active;
} Bullet;

#define MAX_BULLETS 50
Bullet bullets[MAX_BULLETS];

// ========== SPRITES ==========
unsigned int* hand_sprite = NULL;
unsigned int* hand2_sprite = NULL;
int hand_w = 128, hand_h = 128;

// ========== WINDOW ==========
unsigned int* screen_buffer;
HWND hwnd;
HDC hdc;
HBITMAP hBitmap;
BITMAPINFO bmi;

// ========== TIMING ==========
double get_time() {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / freq.QuadPart;
}

// ========== LOAD BMP ==========
unsigned int* load_bmp(const char* fname, int* width, int* height) {
    FILE* f = fopen(fname, "rb");
    if (!f) return NULL;
    
    unsigned char header[54];
    if (fread(header, 1, 54, f) != 54) { fclose(f); return NULL; }
    
    if (header[0] != 'B' || header[1] != 'M') { fclose(f); return NULL; }
    
    int w = *(int*)&header[18];
    int h = *(int*)&header[22];
    int bpp = *(short*)&header[28];
    int data_offset = *(int*)&header[10];
    
    if (bpp != 24 && bpp != 32) { fclose(f); return NULL; }
    
    fseek(f, data_offset, SEEK_SET);
    
    int bytes_per_pixel = bpp / 8;
    int stride = w * bytes_per_pixel;
    int padding = (4 - (stride % 4)) % 4;
    
    unsigned int* data = malloc(w * h * 4);
    if (!data) { fclose(f); return NULL; }
    
    for (int y = h-1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            unsigned char b, g, r, a = 255;
            fread(&b, 1, 1, f);
            fread(&g, 1, 1, f);
            fread(&r, 1, 1, f);
            if (bytes_per_pixel == 4) fread(&a, 1, 1, f);
            data[y * w + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
        fseek(f, padding, SEEK_CUR);
    }
    
    fclose(f);
    *width = w;
    *height = h;
    return data;
}

// ========== CREATE DEFAULT SPRITES ==========
unsigned int* create_hand_sprite(int is_shooting, int* w, int* h) {
    *w = 128; *h = 128;
    unsigned int* data = malloc(128 * 128 * 4);
    if (!data) return NULL;
    
    for (int y = 0; y < 128; y++) {
        for (int x = 0; x < 128; x++) {
            unsigned int color = 0;
            int alpha = 0;
            
            // Hand shape (simple)
            if (x > 20 && x < 100 && y > 50 && y < 90) {
                // Skin color
                int r = 200, g = 150, b = 100;
                alpha = 255;
                color = (alpha << 24) | (r << 16) | (g << 8) | b;
            }
            
            // Fingers
            if ((x > 25 && x < 40 && y > 25 && y < 55) ||
                (x > 45 && x < 60 && y > 25 && y < 55) ||
                (x > 65 && x < 80 && y > 25 && y < 55)) {
                alpha = 255;
                color = (alpha << 24) | (200 << 16) | (150 << 8) | 100;
            }
            
            // Gun
            if (x > 70 && x < 120 && y > 60 && y < 68) {
                alpha = 255;
                color = (alpha << 24) | (60 << 16) | (60 << 8) | 60;
            }
            if (x > 70 && x < 120 && y > 68 && y < 75) {
                alpha = 255;
                color = (alpha << 24) | (100 << 16) | (100 << 8) | 100;
            }
            
            // Trigger guard
            if (x > 65 && x < 75 && y > 85 && y < 95) {
                alpha = 255;
                color = (alpha << 24) | (100 << 16) | (80 << 8) | 60;
            }
            
            // Muzzle flash (shooting)
            if (is_shooting && x > 115 && x < 130 && y > 60 && y < 80) {
                // Yellow flash with glow
                int dist = abs(x - 122) + abs(y - 70);
                if (dist < 15) {
                    alpha = 255;
                    int brightness = 255 - dist * 10;
                    if (brightness < 0) brightness = 0;
                    color = (alpha << 24) | (255 << 16) | (brightness << 8) | (brightness / 2);
                }
            }
            
            data[y * 128 + x] = color;
        }
    }
    return data;
}

// ========== SHOOTING ==========
void shoot_bullet() {
    if (player.ammo <= 0 || player.shooting) return;
    player.ammo--;
    player.shooting = 1;
    player.shoot_timer = 0.2f;
    
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].x = player.x;
            bullets[i].y = player.y;
            bullets[i].dx = player.dir_x * 25.0f;
            bullets[i].dy = player.dir_y * 25.0f;
            bullets[i].life = 5.0f;
            bullets[i].active = 1;
            break;
        }
    }
}

void update_bullets(float dt) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        
        bullets[i].x += bullets[i].dx * dt;
        bullets[i].y += bullets[i].dy * dt;
        bullets[i].life -= dt;
        
        int map_x = (int)bullets[i].x;
        int map_y = (int)bullets[i].y;
        if (map_x < 0 || map_x >= MAP_W || map_y < 0 || map_y >= MAP_H || map[map_x][map_y] > 0) {
            bullets[i].active = 0;
            continue;
        }
        
        // Check enemy collision
        for (int e = 0; e < num_enemies; e++) {
            if (!enemies[e].active || enemies[e].health <= 0) continue;
            float dx = bullets[i].x - enemies[e].x;
            float dy = bullets[i].y - enemies[e].y;
            if (dx*dx + dy*dy < 0.5f) {
                enemies[e].health -= 10;
                bullets[i].active = 0;
                if (enemies[e].health <= 0) {
                    enemies[e].active = 0;
                }
                break;
            }
        }
        
        if (bullets[i].life <= 0) bullets[i].active = 0;
    }
}

// ========== ENEMY AI ==========
void spawn_enemy(float x, float y) {
    if (num_enemies >= MAX_ENEMIES) return;
    enemies[num_enemies].x = x;
    enemies[num_enemies].y = y;
    enemies[num_enemies].health = 20;
    enemies[num_enemies].active = 1;
    enemies[num_enemies].last_shot = 0;
    num_enemies++;
}

void update_enemies(float dt) {
    for (int i = 0; i < num_enemies; i++) {
        if (!enemies[i].active || enemies[i].health <= 0) continue;
        
        float dx = player.x - enemies[i].x;
        float dy = player.y - enemies[i].y;
        float dist = sqrt(dx*dx + dy*dy);
        
        if (dist < 8.0f) {
            // Move toward player
            enemies[i].x += (dx / dist) * 0.8f * dt;
            enemies[i].y += (dy / dist) * 0.8f * dt;
            
            // Attack
            if (dist < 2.0f) {
                enemies[i].last_shot += dt;
                if (enemies[i].last_shot > 1.0f) {
                    player.health -= 5;
                    enemies[i].last_shot = 0;
                }
            }
        }
    }
}

// ========== RAYCASTING ==========
void cast_ray(float ray_dir_x, float ray_dir_y, int* hit, float* perp_dist, int* side, int* map_x_out, int* map_y_out) {
    int map_x = (int)player.x;
    int map_y = (int)player.y;
    
    float delta_dist_x = fabs(1.0f / ray_dir_x);
    float delta_dist_y = fabs(1.0f / ray_dir_y);
    
    float step_x, step_y;
    float side_dist_x, side_dist_y;
    
    if (ray_dir_x < 0) {
        step_x = -1;
        side_dist_x = (player.x - map_x) * delta_dist_x;
    } else {
        step_x = 1;
        side_dist_x = (map_x + 1.0f - player.x) * delta_dist_x;
    }
    
    if (ray_dir_y < 0) {
        step_y = -1;
        side_dist_y = (player.y - map_y) * delta_dist_y;
    } else {
        step_y = 1;
        side_dist_y = (map_y + 1.0f - player.y) * delta_dist_y;
    }
    
    *hit = 0;
    while (!*hit) {
        if (side_dist_x < side_dist_y) {
            side_dist_x += delta_dist_x;
            map_x += step_x;
            *side = 0;
        } else {
            side_dist_y += delta_dist_y;
            map_y += step_y;
            *side = 1;
        }
        if (map_x < 0 || map_x >= MAP_W || map_y < 0 || map_y >= MAP_H) break;
        if (map[map_x][map_y] > 0) *hit = 1;
    }
    
    *map_x_out = map_x;
    *map_y_out = map_y;
    
    if (*hit) {
        if (*side == 0)
            *perp_dist = (map_x - player.x + (1 - step_x) / 2) / ray_dir_x;
        else
            *perp_dist = (map_y - player.y + (1 - step_y) / 2) / ray_dir_y;
        if (*perp_dist < 0.01f) *perp_dist = 0.01f;
    }
}

// ========== SPRITE RENDERING ==========
void render_sprite(float x, float y, unsigned int color, float size) {
    float dx = x - player.x;
    float dy = y - player.y;
    float inv_det = 1.0f / (player.plane_x * player.dir_y - player.dir_x * player.plane_y);
    float transform_x = inv_det * (player.dir_y * dx - player.dir_x * dy);
    float transform_y = inv_det * (-player.plane_y * dx + player.plane_x * dy);
    
    if (transform_y <= 0.1f) return;
    
    int sprite_screen_x = (int)((SCREEN_W / 2) * (1 + transform_x / transform_y));
    int sprite_height = abs((int)(SCREEN_H / transform_y));
    int sprite_width = sprite_height;
    
    int draw_start_y = -sprite_height / 2 + SCREEN_H / 2;
    if (draw_start_y < 0) draw_start_y = 0;
    int draw_end_y = sprite_height / 2 + SCREEN_H / 2;
    if (draw_end_y >= SCREEN_H) draw_end_y = SCREEN_H - 1;
    
    int draw_start_x = -sprite_width / 2 + sprite_screen_x;
    if (draw_start_x < 0) draw_start_x = 0;
    int draw_end_x = sprite_width / 2 + sprite_screen_x;
    if (draw_end_x >= SCREEN_W) draw_end_x = SCREEN_W - 1;
    
    for (int y_pixel = draw_start_y; y_pixel < draw_end_y; y_pixel++) {
        for (int x_pixel = draw_start_x; x_pixel < draw_end_x; x_pixel++) {
            if (x_pixel >= 0 && x_pixel < SCREEN_W && y_pixel >= 0 && y_pixel < SCREEN_H) {
                float shade = 1.0f - transform_y * 0.02f;
                if (shade < 0.1f) shade = 0.1f;
                int r = (color >> 16) & 0xFF;
                int g = (color >> 8) & 0xFF;
                int b = color & 0xFF;
                r = (int)(r * shade);
                g = (int)(g * shade);
                b = (int)(b * shade);
                screen_buffer[y_pixel * SCREEN_W + x_pixel] = (0xFF << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }
}

// ========== RENDER ==========
void render_frame() {
    memset(screen_buffer, 0, SCREEN_W * SCREEN_H * 4);
    
    // Cast rays and draw walls
    for (int x = 0; x < SCREEN_W; x++) {
        float camera_x = 2.0f * x / (float)SCREEN_W - 1.0f;
        float ray_dir_x = player.dir_x + player.plane_x * camera_x;
        float ray_dir_y = player.dir_y + player.plane_y * camera_x;
        
        int hit, side, map_x, map_y;
        float perp_dist;
        cast_ray(ray_dir_x, ray_dir_y, &hit, &perp_dist, &side, &map_x, &map_y);
        
        int line_height = (int)(SCREEN_H / perp_dist);
        int draw_start = -line_height / 2 + SCREEN_H / 2;
        if (draw_start < 0) draw_start = 0;
        int draw_end = line_height / 2 + SCREEN_H / 2;
        if (draw_end >= SCREEN_H) draw_end = SCREEN_H - 1;
        
        unsigned int color;
        if (hit) {
            // Wall color based on map value
            int wall_type = map[map_x][map_y];
            switch(wall_type) {
                case 1: color = (side == 0) ? 0xFF4488FF : 0xFF3366CC; break;  // Blue
                case 2: color = (side == 0) ? 0xFF44CC44 : 0xFF22AA22; break;  // Green
                case 3: color = (side == 0) ? 0xFFFF4444 : 0xFFCC2222; break;  // Red
                case 4: color = (side == 0) ? 0xFFFFCC44 : 0xFFCC9922; break;  // Yellow
                default: color = 0xFFFF00FF; break;
            }
            
            // Distance shading (fog)
            float shade = 1.0f - perp_dist * 0.02f;
            if (shade < 0.1f) shade = 0.1f;
            int r = (color >> 16) & 0xFF;
            int g = (color >> 8) & 0xFF;
            int b = color & 0xFF;
            r = (int)(r * shade);
            g = (int)(g * shade);
            b = (int)(b * shade);
            color = (0xFF << 24) | (r << 16) | (g << 8) | b;
        } else {
            // Sky and floor
            for (int y = 0; y < SCREEN_H; y++) {
                if (y < SCREEN_H / 2) {
                    // Sky gradient
                    float t = (float)y / (SCREEN_H / 2);
                    int r = (int)(135 * (1 - t) + 30 * t);
                    int g = (int)(206 * (1 - t) + 30 * t);
                    int b = (int)(235 * (1 - t) + 80 * t);
                    screen_buffer[y * SCREEN_W + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
                } else {
                    // Floor gradient
                    float t = (float)(y - SCREEN_H / 2) / (SCREEN_H / 2);
                    int r = (int)(80 * (1 - t * 0.5f));
                    int g = (int)(60 * (1 - t * 0.5f));
                    int b = (int)(40 * (1 - t * 0.5f));
                    screen_buffer[y * SCREEN_W + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
                }
            }
            continue;
        }
        
        // Draw wall
        for (int y = draw_start; y < draw_end; y++) {
            screen_buffer[y * SCREEN_W + x] = color;
        }
    }
    
    // Draw enemies as sprites
    for (int i = 0; i < num_enemies; i++) {
        if (!enemies[i].active || enemies[i].health <= 0) continue;
        float dx = player.x - enemies[i].x;
        float dy = player.y - enemies[i].y;
        if (dx*dx + dy*dy > 400.0f) continue;
        unsigned int color = (enemies[i].health > 10) ? 0xFFFF4444 : 0xFFFF8888;
        render_sprite(enemies[i].x, enemies[i].y, color, 0.5f);
    }
    
    // Draw bullets
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        render_sprite(bullets[i].x, bullets[i].y, 0xFFFFFF00, 0.1f);
    }
    
    // Draw hand sprite
    unsigned int* hand = (player.shooting) ? hand2_sprite : hand_sprite;
    if (hand) {
        int sx = SCREEN_W - 128 - 20;
        int sy = SCREEN_H - 128 - 20;
        for (int y = 0; y < 128 && y + sy < SCREEN_H; y++) {
            for (int x = 0; x < 128 && x + sx < SCREEN_W; x++) {
                unsigned int color = hand[y * 128 + x];
                unsigned char alpha = (color >> 24) & 0xFF;
                if (alpha == 0) continue;
                screen_buffer[(sy + y) * SCREEN_W + (sx + x)] = color;
            }
        }
    }
    
    // HUD
    char hud[200];
    sprintf(hud, "Health: %d  Ammo: %d  Enemies: %d", player.health, player.ammo, num_enemies);
    TextOut(hdc, 10, 10, hud, strlen(hud));
}

// ========== INPUT ==========
void handle_input(float dt) {
    float move = MOVE_SPEED * dt;
    float rot = ROT_SPEED * dt;
    
    // Update shooting timer
    if (player.shooting) {
        player.shoot_timer -= dt;
        if (player.shoot_timer <= 0) player.shooting = 0;
    }
    
    // Movement with collision
    if (GetAsyncKeyState('W') & 0x8000) {
        int nx = (int)(player.x + player.dir_x * move);
        int ny = (int)(player.y + player.dir_y * move);
        if (nx >= 0 && nx < MAP_W && map[nx][(int)player.y] == 0) player.x += player.dir_x * move;
        if (ny >= 0 && ny < MAP_H && map[(int)player.x][ny] == 0) player.y += player.dir_y * move;
    }
    if (GetAsyncKeyState('S') & 0x8000) {
        int nx = (int)(player.x - player.dir_x * move);
        int ny = (int)(player.y - player.dir_y * move);
        if (nx >= 0 && nx < MAP_W && map[nx][(int)player.y] == 0) player.x -= player.dir_x * move;
        if (ny >= 0 && ny < MAP_H && map[(int)player.x][ny] == 0) player.y -= player.dir_y * move;
    }
    
    // Rotation
    if (GetAsyncKeyState('A') & 0x8000) {
        float old_dir_x = player.dir_x;
        player.dir_x = player.dir_x * cos(-rot) - player.dir_y * sin(-rot);
        player.dir_y = old_dir_x * sin(-rot) + player.dir_y * cos(-rot);
        float old_plane_x = player.plane_x;
        player.plane_x = player.plane_x * cos(-rot) - player.plane_y * sin(-rot);
        player.plane_y = old_plane_x * sin(-rot) + player.plane_y * cos(-rot);
    }
    if (GetAsyncKeyState('D') & 0x8000) {
        float old_dir_x = player.dir_x;
        player.dir_x = player.dir_x * cos(rot) - player.dir_y * sin(rot);
        player.dir_y = old_dir_x * sin(rot) + player.dir_y * cos(rot);
        float old_plane_x = player.plane_x;
        player.plane_x = player.plane_x * cos(rot) - player.plane_y * sin(rot);
        player.plane_y = old_plane_x * sin(rot) + player.plane_y * cos(rot);
    }
    
    // Shoot
    static int last_shot = 0;
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
        if (!last_shot && !player.shooting) {
            shoot_bullet();
            last_shot = 1;
        }
    } else {
        last_shot = 0;
    }
}

// ========== WINDOW ==========
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) PostQuitMessage(0);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// ========== MAIN ==========
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    srand(time(NULL));
    
    // Load or create sprites
    hand_sprite = load_bmp("hand.bmp", &hand_w, &hand_h);
    if (!hand_sprite) {
        hand_sprite = create_hand_sprite(0, &hand_w, &hand_h);
        printf("Created default hand.bmp\n");
    }
    
    hand2_sprite = load_bmp("hand2.bmp", &hand_w, &hand_h);
    if (!hand2_sprite) {
        hand2_sprite = create_hand_sprite(1, &hand_w, &hand_h);
        printf("Created default hand2.bmp\n");
    }
    
    // Spawn enemies
    spawn_enemy(5.0f, 5.0f);
    spawn_enemy(12.0f, 3.0f);
    spawn_enemy(8.0f, 15.0f);
    spawn_enemy(3.0f, 12.0f);
    spawn_enemy(15.0f, 8.0f);
    
    // Create window
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "Wolf3D";
    RegisterClass(&wc);
    
    hwnd = CreateWindow("Wolf3D", "Wolfenstein 3D Engine",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100, SCREEN_W + 16, SCREEN_H + 39,
        NULL, NULL, hInstance, NULL);
    
    // Create bitmap
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = SCREEN_W;
    bmi.bmiHeader.biHeight = -SCREEN_H;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = SCREEN_W * SCREEN_H * 4;
    
    hdc = GetDC(hwnd);
    hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&screen_buffer, NULL, 0);
    SelectObject(hdc, hBitmap);
    
    double last_time = get_time();
    MSG msg;
    
    while (1) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                DeleteObject(hBitmap);
                ReleaseDC(hwnd, hdc);
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        double current_time = get_time();
        float dt = (float)(current_time - last_time);
        if (dt > 0.05f) dt = 0.05f;
        
        if (dt > 0.0f) {
            handle_input(dt);
            update_bullets(dt);
            update_enemies(dt);
            render_frame();
            BitBlt(hdc, 0, 0, SCREEN_W, SCREEN_H, hdc, 0, 0, SRCCOPY);
            last_time = current_time;
        }
    }
}

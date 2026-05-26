#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <string>
#include <cmath>
#include <iostream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

using namespace std;

// --- СТРУКТУРЫ ДАННЫХ ---
// В SDL2 координаты лучше хранить во float для плавного движения, 
// а при отрисовке приводить к int (SDL_Rect)
struct Bullet {
    float x, y, w, h;
    float vx, vy;
    float angle; // Для поворота текстуры пули
};

struct Enemy {
    float x, y, w, h;
    float speed;
    int hp;
};

enum DropType { HEALTH, AMMO, RAPID_FIRE };

struct Drop {
    float x, y, w, h;
    DropType type;
    int lifetime;
};

const float PI = 3.14159265f;

// Вспомогательная функция для проверки столкновений (AABB)
bool checkCollision(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2) {
    return x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && y1 + h1 > y2;
}

// --- ГЛАВНЫЙ КЛАСС ИГРЫ ---
class SwampGame {
public:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    SDL_Texture *texBg = nullptr, *texPlayer = nullptr, *texZombie = nullptr, *texBullet = nullptr;
    SDL_Texture *texDropHealth = nullptr, *texDropAmmo = nullptr, *texDropRapid = nullptr;

    TTF_Font* font = nullptr;

    // Игровые сущности
    float playerX = 40.f, playerY = 300.f, playerW = 60.f, playerH = 60.f;

    vector<Bullet> bullets;
    vector<Enemy> enemies;
    vector<Drop> drops;

    int hp = 100;
    int score = 0;
    int ammo = 30;
    int fireCooldown = 0;
    int buffTimer = 0;
    bool isGameOver = false;

    int wave = 1;
    int enemiesPerWave = 5;
    int enemiesSpawned = 0;
    int spawnTimer = 0;

    // Состояние мыши
    int mouseX = 0, mouseY = 0;
    bool mouseDown = false;
    bool prevMouseDown = false;

    SwampGame() {
        // Инициализация SDL2
        SDL_Init(SDL_INIT_VIDEO);
        IMG_Init(IMG_INIT_PNG);
        TTF_Init();

        window = SDL_CreateWindow("Swamp Attack: Zombie Edition", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

        // Загрузка ресурсов
        texBg = loadTexture("assets/background.png");
        texPlayer = loadTexture("assets/player.png");
        texZombie = loadTexture("assets/zombie.png");
        texBullet = loadTexture("assets/bullet.png");

        texDropHealth = loadTexture("assets/health.png");
        texDropAmmo = loadTexture("assets/ammo.png");
        texDropRapid = loadTexture("assets/rapid.png");

        font = TTF_OpenFont("assets/arial.ttf", 20);
    }

    ~SwampGame() {
        if (texBg) SDL_DestroyTexture(texBg);
        if (texPlayer) SDL_DestroyTexture(texPlayer);
        if (texZombie) SDL_DestroyTexture(texZombie);
        if (texBullet) SDL_DestroyTexture(texBullet);
        if (texDropHealth) SDL_DestroyTexture(texDropHealth);
        if (texDropAmmo) SDL_DestroyTexture(texDropAmmo);
        if (texDropRapid) SDL_DestroyTexture(texDropRapid);
        
        if (font) TTF_CloseFont(font);
        
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
    }

    SDL_Texture* loadTexture(const string& path) {
        SDL_Surface* surface = IMG_Load(path.c_str());
        if (!surface) return nullptr;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        return tex;
    }

    void renderText(const string& text, int x, int y, SDL_Color color, int fontSize = 20) {
        if (!font) return;
        
        // Если нужен другой размер шрифта (для Game Over), в SDL_ttf проще всего отмасштабировать текстуру
        SDL_Surface* surface = TTF_RenderUTF8_Solid(font, text.c_str(), color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect dest = { x, y, surface->w, surface->h };
            
            if (fontSize != 20) {
                float scale = (float)fontSize / 20.0f;
                dest.w = (int)(dest.w * scale);
                dest.h = (int)(dest.h * scale);
            }

            SDL_RenderCopy(renderer, texture, nullptr, &dest);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
        }
    }

    void update() {
        prevMouseDown = mouseDown;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                // В вебе это не сработает, но для десктопа полезно
                exit(0); 
            }
        }

        // Получаем состояние мыши
        Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);
        mouseDown = (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        bool mouseClicked = mouseDown && !prevMouseDown;

        // Логика игры
        if (isGameOver) {
            // Рестарт не прописан явно в старом коде, просто висим
        } else {
            // Клик по дропам (нужно именно нажатие, а не удержание)
            if (mouseClicked) {
                for (size_t i = 0; i < drops.size(); i++) {
                    if (mouseX >= drops[i].x && mouseX <= drops[i].x + drops[i].w &&
                        mouseY >= drops[i].y && mouseY <= drops[i].y + drops[i].h) {
                        
                        if (drops[i].type == HEALTH) hp = min(100, hp + 20);
                        else if (drops[i].type == AMMO) ammo += 20;
                        else if (drops[i].type == RAPID_FIRE) buffTimer = 300;

                        score += 5;
                        drops.erase(drops.begin() + i);
                        break;
                    }
                }
            }

            if (fireCooldown > 0) fireCooldown--;
            if (buffTimer > 0) buffTimer--;

            // Стрельба (удержание мыши)
            if (mouseDown && fireCooldown <= 0) {
                if (ammo > 0 || buffTimer > 0) {
                    if (buffTimer == 0) ammo--;

                    Bullet b;
                    b.w = 20.f; b.h = 8.f;
                    b.x = playerX + 50.f; 
                    b.y = playerY + 30.f;

                    float dx = mouseX - b.x;
                    float dy = mouseY - b.y;
                    float length = sqrt(dx * dx + dy * dy);

                    if (length != 0) {
                        b.vx = (dx / length) * 20.f;
                        b.vy = (dy / length) * 20.f;
                        b.angle = atan2(dy, dx) * 180.f / PI;
                        bullets.push_back(b);
                    }

                    fireCooldown = (buffTimer > 0) ? 5 : 20;
                }
            }

            // Движение пуль
            for (size_t i = 0; i < bullets.size(); i++) {
                bullets[i].x += bullets[i].vx;
                bullets[i].y += bullets[i].vy;
                if (bullets[i].x > 800 || bullets[i].x < 0 || bullets[i].y > 600 || bullets[i].y < 0) {
                    bullets.erase(bullets.begin() + i);
                    i--;
                }
            }

            // Спавн врагов
            if (enemiesSpawned < enemiesPerWave) {
                spawnTimer++;
                int spawnRate = max(20, 60 - (wave * 5));

                if (spawnTimer >= spawnRate) {
                    Enemy e;
                    float size = 40.f + (rand() % 20);
                    e.w = size; e.h = size;
                    e.x = 800.f;
                    e.y = static_cast<float>(rand() % 450 + 50);
                    e.speed = 1.0f + static_cast<float>(rand() % 10) / 10.f + (wave * 0.2f);
                    e.hp = 10 + (wave * 5);

                    enemies.push_back(e);
                    enemiesSpawned++;
                    spawnTimer = 0;
                }
            }
            else if (enemies.empty()) {
                wave++;
                enemiesPerWave += 3;
                enemiesSpawned = 0;
                ammo += 15;
            }

            // Движение и столкновения врагов
            for (size_t i = 0; i < enemies.size(); i++) {
                enemies[i].x -= enemies[i].speed;

                // Враг дошел до базы
                if (enemies[i].x <= 40.f) {
                    hp -= 15;
                    enemies.erase(enemies.begin() + i);
                    i--;
                    if (hp <= 0) isGameOver = true;
                    continue;
                }

                // Попадание пули во врага
                bool hit = false;
                for (size_t j = 0; j < bullets.size(); j++) {
                    if (checkCollision(enemies[i].x, enemies[i].y, enemies[i].w, enemies[i].h,
                                       bullets[j].x, bullets[j].y, bullets[j].w, bullets[j].h)) {
                        enemies[i].hp -= (buffTimer > 0) ? 20 : 10;
                        bullets.erase(bullets.begin() + j);
                        hit = true;
                        break;
                    }
                }

                // Смерть врага
                if (hit && enemies[i].hp <= 0) {
                    score += 10;

                    // Выпадение дропа
                    if (rand() % 100 < 30) {
                        Drop d;
                        d.w = 25.f; d.h = 25.f;
                        d.x = enemies[i].x;
                        d.y = enemies[i].y;
                        d.lifetime = 300;

                        int dropChance = rand() % 100;
                        if (dropChance < 40) d.type = AMMO;
                        else if (dropChance < 80) d.type = HEALTH;
                        else d.type = RAPID_FIRE;

                        drops.push_back(d);
                    }

                    enemies.erase(enemies.begin() + i);
                    i--;
                }
            }

            // Жизнь дропов
            for (size_t i = 0; i < drops.size(); i++) {
                drops[i].lifetime--;
                if (drops[i].lifetime <= 0) {
                    drops.erase(drops.begin() + i);
                    i--;
                }
            }
        }

        // === ОТРИСОВКА ===
        
        // Фон
        if (texBg) {
            SDL_RenderCopy(renderer, texBg, nullptr, nullptr);
        } else {
            SDL_SetRenderDrawColor(renderer, 50, 50, 70, 255);
            SDL_RenderClear(renderer);
        }

        // Игрок (база)
        SDL_Rect pRect = { (int)playerX, (int)playerY, (int)playerW, (int)playerH };
        if (texPlayer) {
            SDL_RenderCopy(renderer, texPlayer, nullptr, &pRect);
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &pRect);
        }

        // Дропы
        for (auto& d : drops) {
            SDL_Rect r = { (int)d.x, (int)d.y, (int)d.w, (int)d.h };
            
            // Мигание перед исчезновением
            if (d.lifetime < 60 && d.lifetime % 10 < 5) continue; 

            SDL_Texture* t = nullptr;
            if (d.type == HEALTH) { t = texDropHealth; SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); }
            else if (d.type == AMMO) { t = texDropAmmo; SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255); }
            else { t = texDropRapid; SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255); }

            if (t) SDL_RenderCopy(renderer, t, nullptr, &r);
            else SDL_RenderFillRect(renderer, &r);
        }

        // Пули
        for (auto& b : bullets) {
            SDL_Rect r = { (int)b.x, (int)b.y, (int)b.w, (int)b.h };
            if (texBullet) {
                // В SDL2 есть встроенная функция для поворота текстуры!
                SDL_RenderCopyEx(renderer, texBullet, nullptr, &r, b.angle, nullptr, SDL_FLIP_NONE);
            } else {
                SDL_SetRenderDrawColor(renderer, buffTimer > 0 ? 0 : 255, 255, buffTimer > 0 ? 255 : 0, 255);
                SDL_RenderFillRect(renderer, &r);
            }
        }

        // Враги
        for (auto& e : enemies) {
            SDL_Rect r = { (int)e.x, (int)e.y, (int)e.w, (int)e.h };
            if (texZombie) {
                SDL_RenderCopy(renderer, texZombie, nullptr, &r);
            } else {
                SDL_SetRenderDrawColor(renderer, 100, 255, 100, 255);
                SDL_RenderFillRect(renderer, &r);
            }
        }

        // Интерфейс: Полоска здоровья
        SDL_Rect hpBg = { 18, 18, 204, 24 };
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(renderer, &hpBg);

        int drawHp = max(0, hp);
        SDL_Rect hpFg = { 20, 20, drawHp * 2, 20 };
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderFillRect(renderer, &hpFg);

        // Текст
        string buffText = (buffTimer > 0) ? "  [MACHINEGUN ACTIVE!]" : "";
        string uiStr = "Wave: " + to_string(wave) + "  |  Score: " + to_string(score) + "  |  Ammo: " + to_string(ammo) + buffText;
        SDL_Color white = {255, 255, 255, 255};
        renderText(uiStr, 240, 15, white);

        if (isGameOver) {
            SDL_Color red = {255, 0, 0, 255};
            renderText("GAME OVER!", 250, 200, red, 50);
            renderText("Refresh page to restart", 260, 260, white, 30);
        }

        // Вывод кадра на экран
        SDL_RenderPresent(renderer);
    }
};

// Глобальный указатель
SwampGame* game = nullptr;

// Функция-обёртка для Emscripten
void mainLoop() {
    game->update();
}

int main(int argc, char* argv[]) {
    srand(static_cast<unsigned int>(time(NULL)));
    game = new SwampGame();

#ifdef __EMSCRIPTEN__
    // Запускаем бесконечный цикл через браузерный requestAnimationFrame
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    // Запасной вариант для десктопа (если захотите собрать обычный .exe в будущем)
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
        }
        mainLoop();
        SDL_Delay(16); // Примерно 60 FPS
    }
#endif

    delete game;
    return 0;
}
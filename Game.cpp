#include <SFML/Graphics.hpp>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <string>
#include <cmath>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

using namespace sf;
using namespace std;

// --- СТРУКТУРЫ ДАННЫХ ---
struct Bullet {
    RectangleShape shape;
    float vx, vy;
};

struct Enemy {
    RectangleShape shape;
    float speed;
    int hp;
};

enum DropType { HEALTH, AMMO, RAPID_FIRE };

struct Drop {
    RectangleShape shape;
    DropType type;
    int lifetime;
};

const float PI = 3.14159265f;

// --- ГЛАВНЫЙ КЛАСС ИГРЫ ---
class SwampGame {
public:
    RenderWindow window;

    Texture texBg, texPlayer, texZombie, texBullet;
    Texture texDropHealth, texDropAmmo, texDropRapid;
    bool hasBg, hasPlayer, hasZombie, hasBullet;
    bool hasDropHealth, hasDropAmmo, hasDropRapid;

    Font font;
    bool hasFont;
    Text uiText;
    Text gameOverText;

    RectangleShape background, player, hpBarBg, hpBar;

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

    SwampGame() : window(VideoMode(800, 600), "Swamp Attack: Zombie Edition") {
        window.setFramerateLimit(60);

        // В Emscripten пути будут искаться в корне виртуальной ФС, куда мы их запакуем
        hasBg = texBg.loadFromFile("assets/background.png");
        hasPlayer = texPlayer.loadFromFile("assets/player.png");
        hasZombie = texZombie.loadFromFile("assets/zombie.png");
        hasBullet = texBullet.loadFromFile("assets/bullet.png");

        hasDropHealth = texDropHealth.loadFromFile("assets/health.png");
        hasDropAmmo = texDropAmmo.loadFromFile("assets/ammo.png");
        hasDropRapid = texDropRapid.loadFromFile("assets/rapid.png");

        hasFont = font.loadFromFile("assets/arial.ttf");

        if (hasFont) {
            uiText.setFont(font);
            uiText.setCharacterSize(20);
            uiText.setFillColor(Color::White);
            uiText.setPosition(240.f, 15.f);

            gameOverText.setFont(font);
            gameOverText.setCharacterSize(50);
            gameOverText.setFillColor(Color::Red);
            gameOverText.setPosition(150.f, 250.f);
        }

        background.setSize(Vector2f(800.f, 600.f));
        background.setFillColor(Color(50, 50, 70));
        if (hasBg) background.setTexture(&texBg);

        player.setSize(Vector2f(60.f, 60.f));
        player.setPosition(40.f, 300.f);
        if (hasPlayer) player.setTexture(&texPlayer);

        hpBarBg.setSize(Vector2f(204.f, 24.f));
        hpBarBg.setFillColor(Color(50, 50, 50));
        hpBarBg.setPosition(18.f, 18.f);

        hpBar.setSize(Vector2f(200.f, 20.f));
        hpBar.setFillColor(Color::Red);
        hpBar.setPosition(20.f, 20.f);
    }

    void update() {
        Event event;
        while (window.pollEvent(event)) {
            if (event.type == Event::Closed)
                window.close();

            if (isGameOver && event.type == Event::KeyPressed && event.key.code == Keyboard::Enter) {
                // В браузере window.close() работает специфично, но для десктопа оставляем
                window.close();
                // Можно добавить рестарт игры
            }

            if (!isGameOver && event.type == Event::MouseButtonPressed && event.mouseButton.button == Mouse::Left) {
                Vector2f mousePos(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));

                for (size_t i = 0; i < drops.size(); i++) {
                    if (drops[i].shape.getGlobalBounds().contains(mousePos)) {
                        if (drops[i].type == HEALTH) hp = min(100, hp + 20);
                        else if (drops[i].type == AMMO) ammo += 20;
                        else if (drops[i].type == RAPID_FIRE) buffTimer = 300;

                        score += 5;
                        drops.erase(drops.begin() + i);
                        break;
                    }
                }
            }
        }

        if (!isGameOver) {
            if (fireCooldown > 0) fireCooldown--;
            if (buffTimer > 0) buffTimer--;

            if (Mouse::isButtonPressed(Mouse::Left) && fireCooldown <= 0) {
                if (ammo > 0 || buffTimer > 0) {
                    if (buffTimer == 0) ammo--;

                    Bullet b;
                    b.shape.setSize(Vector2f(20.f, 8.f));
                    b.shape.setFillColor(buffTimer > 0 ? Color::Cyan : Color::Yellow);
                    if (hasBullet) b.shape.setTexture(&texBullet);

                    Vector2f startPos = player.getPosition() + Vector2f(50.f, 30.f);
                    b.shape.setPosition(startPos);

                    Vector2f mousePos(static_cast<float>(Mouse::getPosition(window).x), static_cast<float>(Mouse::getPosition(window).y));
                    float dx = mousePos.x - startPos.x;
                    float dy = mousePos.y - startPos.y;
                    float length = sqrt(dx * dx + dy * dy);

                    if (length != 0) {
                        b.vx = (dx / length) * 20.f;
                        b.vy = (dy / length) * 20.f;
                        float angle = atan2(dy, dx) * 180.f / PI;
                        b.shape.setRotation(angle);
                        bullets.push_back(b);
                    }

                    fireCooldown = (buffTimer > 0) ? 5 : 20;
                }
            }

            for (size_t i = 0; i < bullets.size(); i++) {
                bullets[i].shape.move(bullets[i].vx, bullets[i].vy);
                Vector2f pos = bullets[i].shape.getPosition();
                if (pos.x > 800 || pos.x < 0 || pos.y > 600 || pos.y < 0) {
                    bullets.erase(bullets.begin() + i);
                    i--;
                }
            }

            if (enemiesSpawned < enemiesPerWave) {
                spawnTimer++;
                int spawnRate = max(20, 60 - (wave * 5));

                if (spawnTimer >= spawnRate) {
                    Enemy e;
                    float size = 40.f + (rand() % 20);
                    e.shape.setSize(Vector2f(size, size));
                    e.shape.setFillColor(Color(100, 255, 100));
                    if (hasZombie) e.shape.setTexture(&texZombie);

                    e.shape.setPosition(800.f, static_cast<float>(rand() % 450 + 50));
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

            for (size_t i = 0; i < enemies.size(); i++) {
                enemies[i].shape.move(-enemies[i].speed, 0.f);

                if (enemies[i].shape.getPosition().x <= 40.f) {
                    hp -= 15;
                    enemies.erase(enemies.begin() + i);
                    i--;

                    if (hp >= 0) hpBar.setSize(Vector2f(hp * 2.f, 20.f));
                    if (hp <= 0) isGameOver = true;
                    continue;
                }

                bool hit = false;
                for (size_t j = 0; j < bullets.size(); j++) {
                    if (enemies[i].shape.getGlobalBounds().intersects(bullets[j].shape.getGlobalBounds())) {
                        enemies[i].hp -= (buffTimer > 0) ? 20 : 10;
                        bullets.erase(bullets.begin() + j);
                        hit = true;
                        break;
                    }
                }

                if (hit && enemies[i].hp <= 0) {
                    score += 10;

                    if (rand() % 100 < 30) {
                        Drop d;
                        d.shape.setSize(Vector2f(25.f, 25.f));
                        d.shape.setPosition(enemies[i].shape.getPosition());
                        d.lifetime = 300;

                        int dropChance = rand() % 100;
                        if (dropChance < 40) {
                            d.type = AMMO;
                            if (hasDropAmmo) { d.shape.setTexture(&texDropAmmo); d.shape.setFillColor(Color::White); }
                            else d.shape.setFillColor(Color::Yellow);
                        }
                        else if (dropChance < 80) {
                            d.type = HEALTH;
                            if (hasDropHealth) { d.shape.setTexture(&texDropHealth); d.shape.setFillColor(Color::White); }
                            else d.shape.setFillColor(Color::Red);
                        }
                        else {
                            d.type = RAPID_FIRE;
                            if (hasDropRapid) { d.shape.setTexture(&texDropRapid); d.shape.setFillColor(Color::White); }
                            else d.shape.setFillColor(Color::Cyan);
                        }

                        drops.push_back(d);
                    }

                    enemies.erase(enemies.begin() + i);
                    i--;
                }
            }

            for (size_t i = 0; i < drops.size(); i++) {
                drops[i].lifetime--;
                Color c = drops[i].shape.getFillColor();
                if (drops[i].lifetime < 60 && drops[i].lifetime % 10 < 5) {
                    c.a = 0;
                }
                else {
                    c.a = 255;
                }
                drops[i].shape.setFillColor(c);

                if (drops[i].lifetime <= 0) {
                    drops.erase(drops.begin() + i);
                    i--;
                }
            }
        }

        if (hasFont) {
            string buffText = (buffTimer > 0) ? " [MACHINEGUN ACTIVE!]" : "";
            uiText.setString("Wave: " + to_string(wave) + "  |  Score: " + to_string(score) +
                "  |  Ammo: " + to_string(ammo) + buffText);
        }

        window.clear();
        window.draw(background);
        window.draw(player);
        for (auto& d : drops) window.draw(d.shape);
        for (auto& b : bullets) window.draw(b.shape);
        for (auto& e : enemies) window.draw(e.shape);
        window.draw(hpBarBg);
        window.draw(hpBar);
        if (hasFont) window.draw(uiText);

        if (isGameOver && hasFont) {
            gameOverText.setString("GAME OVER!\nWave Reached: " + to_string(wave) + "\nRefresh page to restart");
            window.draw(gameOverText);
        }

        window.display();
    }
};

// Глобальный указатель на игру для Emscripten
SwampGame* game = nullptr;

// Функция-обёртка для главного цикла
void mainLoop() {
    game->update();
}

int main() {
    srand(static_cast<unsigned int>(time(NULL)));
    game = new SwampGame();

#ifdef __EMSCRIPTEN__
    // 0 = бесконечный цикл, 1 = привязать к частоте обновления монитора (requestAnimationFrame)
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while (game->window.isOpen()) {
        mainLoop();
    }
#endif

    delete game;
    return 0;
}
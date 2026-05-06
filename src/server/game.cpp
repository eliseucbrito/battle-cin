#include "../../include/game.h"
#include "../../include/hero_assassin.h"
#include "../../include/hero_support.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

Game::Game()
    : connectedCount_(0), phase_(PHASE_WAITING), phaseTimer_(0.f),
      buffZoneCount_(0), roundWinner_(0xFF), matchWinner_(0xFF)
{
    // Initialize trainers with some default data (could be dynamic later)
    trainers_[0] = Trainer("Prof. Paulo", "Estrutura de Dados", 0, ABILITY_RALLY);
    trainers_[1] = Trainer("Prof. Eliseu", "Orientacao a Objetos", 1, ABILITY_SHIELD_WALL);

    // Give each trainer a set of heroes
    for (int i = 0; i < 2; i++) {
        trainers_[i].addHero(ARCHETYPE_TANK);
        trainers_[i].addHero(ARCHETYPE_FIGHTER);
        trainers_[i].addHero(ARCHETYPE_MAGE);
    }
}

void Game::registerPlayer(int pid, const sockaddr_in& from)
{
    if (pid < 0 || pid > 1) return;
    trainers_[pid].setAddr(from);
    trainers_[pid].resetScore();
    trainers_[pid].resetForRound();
    
    ++connectedCount_;
    printf("Trainer %d (%s) conectado! (%d/2)\n", pid, trainers_[pid].name().c_str(), connectedCount_);
    
    if (connectedCount_ == 2 && phase_ == PHASE_WAITING)
        startPositioning();
}

void Game::handlePlaceHero(int pid, int heroIdx, uint8_t tx, uint8_t ty)
{
    if (phase_ != PHASE_POSITIONING) return;
    if (tx >= GRID_COLS || ty >= GRID_ROWS) return;
    
    bool isLeft = (pid == 0);
    trainers_[pid].placeHero(heroIdx, tx, ty, isLeft);
}

void Game::handleUseAbility(int pid)
{
    if (phase_ != PHASE_BATTLE) return;
    trainers_[pid].useAbility();
}

void Game::update(float dt)
{
    for (int i = 0; i < 2; i++) {
        for (int h = 0; h < trainers_[i].heroCount(); h++) {
            Hero& hero = trainers_[i].heroAt(h);
            hero.tickTimers(dt);
            hero.tickUltimate(dt);
        }
        trainers_[i].tickAbility(dt);
    }

    switch (phase_) {
        case PHASE_POSITIONING:
            phaseTimer_ -= dt;
            if (phaseTimer_ <= 0.f) startBattle();
            break;

        case PHASE_BATTLE:
            phaseTimer_ -= dt;
            autoBattleMove();
            runCombat();
            tickUltimates(dt);
            
            if (!trainers_[0].hasLiveHeroes()) endRound(1);
            else if (!trainers_[1].hasLiveHeroes()) endRound(0);
            else if (phaseTimer_ <= 0.f) resolveTimeLimit();
            break;

        case PHASE_ROUND_END:
            phaseTimer_ -= dt;
            if (phaseTimer_ <= 0.f) startPositioning();
            break;

        default: break;
    }
}

void Game::buildSnapshot(GameSnapshot& snap) const
{
    snap.phase = phase_;
    snap.timer = (uint8_t)(phaseTimer_ < 0 ? 0 : (int)phaseTimer_);
    snap.buffZoneCount = (uint8_t)buffZoneCount_;
    snap.roundWinner = roundWinner_;
    snap.matchWinner = matchWinner_;

    int totalHeroes = 0;
    for (int i = 0; i < 2; i++) {
        const Trainer& t = trainers_[i];
        snap.trainers[i] = {
            t.trainerId(), t.score(), (uint8_t)t.heroCount(), (uint8_t)t.canUseAbility()
        };

        for (int h = 0; h < t.heroCount(); h++) {
            const Hero& hero = t.heroAt(h);
            if (totalHeroes < MAX_HEROES_TOTAL) {
                snap.heroes[totalHeroes++] = {
                    hero.x(), hero.y(),
                    (uint16_t)hero.hp(), (uint16_t)hero.maxHp(),
                    (uint8_t)hero.ad(), (uint8_t)hero.arm(),
                    hero.archetype(), hero.buff(), (uint8_t)hero.alive(),
                    (uint8_t)hero.ultActive(), (uint8_t)i
                };
            }
        }
    }
    snap.heroCount = (uint8_t)totalHeroes;

    for (int i = 0; i < 4; i++) {
        snap.buffZones[i] = (i < buffZoneCount_) ? buffZones_[i] : BuffZoneInfo{0xFF, 0xFF, BUFF_NONE};
    }
}

// ── Private logic ─────────────────────────────────────────────────────────────

void Game::startPositioning()
{
    phase_ = PHASE_POSITIONING;
    phaseTimer_ = POSITIONING_TIME;
    roundWinner_ = 0xFF;

    for (int i = 0; i < 2; i++) {
        trainers_[i].resetForRound();
        // Default positions (horizontal)
        for (int h = 0; h < trainers_[i].heroCount(); h++) {
            trainers_[i].heroAt(h).setPosition(i == 0 ? 1 : 6, 2 + h);
        }
    }
    generateBuffZones();
}

void Game::startBattle()
{
    phase_ = PHASE_BATTLE;
    phaseTimer_ = BATTLE_MAX_TIME;

    // Apply buffs from zones
    for (int i = 0; i < 2; i++) {
        for (int h = 0; h < trainers_[i].heroCount(); h++) {
            Hero& hero = trainers_[i].heroAt(h);
            for (int b = 0; b < buffZoneCount_; b++) {
                if (hero.x() == buffZones_[b].x && hero.y() == buffZones_[b].y) {
                    hero.applyBuff(buffZones_[b].type);
                    break;
                }
            }
        }
    }
}

void Game::endRound(uint8_t winner)
{
    roundWinner_ = winner;
    if (winner != 0xFF) {
        trainers_[winner].addScore();
        if (trainers_[winner].score() >= WIN_SCORE) {
            matchWinner_ = winner;
            phase_ = PHASE_MATCH_END;
            return;
        }
    }
    phase_ = PHASE_ROUND_END;
    phaseTimer_ = ROUND_END_TIME;
}

void Game::autoBattleMove()
{
    for (int i = 0; i < 2; i++) {
        for (int h = 0; h < trainers_[i].heroCount(); h++) {
            Hero& hero = trainers_[i].heroAt(h);
            if (!hero.alive() || hero.moveTimer() > 0.f) continue;

            // Find nearest enemy
            Hero* target = nullptr;
            float minDist = 1000.f;
            
            for (int eh = 0; eh < trainers_[1 - i].heroCount(); eh++) {
                Hero& enemy = trainers_[1 - i].heroAt(eh);
                if (!enemy.alive()) continue;
                
                float d = sqrtf(powf((float)hero.x() - enemy.x(), 2) + powf((float)hero.y() - enemy.y(), 2));
                if (d < minDist) { minDist = d; target = &enemy; }
            }

            if (target && !hero.isAdjacentTo(*target)) {
                int nx, ny;
                if (hero.chooseMove(target->x(), target->y(), nx, ny)) {
                    // Check if cell is occupied by ally
                    bool occupied = false;
                    for (int ah = 0; ah < trainers_[i].heroCount(); ah++) {
                        if (ah == h) continue;
                        if (trainers_[i].heroAt(ah).alive() && trainers_[i].heroAt(ah).x() == nx && trainers_[i].heroAt(ah).y() == ny) {
                            occupied = true; break;
                        }
                    }
                    if (!occupied) {
                        hero.setPosition(nx, ny);
                        hero.startMoveTimer();
                    }
                }
            }
        }
    }
}

void Game::runCombat()
{
    for (int i = 0; i < 2; i++) {
        for (int h = 0; h < trainers_[i].heroCount(); h++) {
            Hero& hero = trainers_[i].heroAt(h);
            if (!hero.alive() || hero.attackTimer() > 0.f) continue;

            // Find adjacent enemy
            for (int eh = 0; eh < trainers_[1 - i].heroCount(); eh++) {
                Hero& enemy = trainers_[1 - i].heroAt(eh);
                if (enemy.alive() && hero.isAdjacentTo(enemy)) {
                    hero.attackTarget(enemy);
                    break; // one attack per tick
                }
            }
        }
    }
}

void Game::tickUltimates(float dt)
{
    for (int i = 0; i < 2; i++) {
        for (int h = 0; h < trainers_[i].heroCount(); h++) {
            Hero& hero = trainers_[i].heroAt(h);
            if (!hero.alive() || !hero.ultReady()) continue;

            // Provide context for special ultimates
            if (hero.archetype() == ARCHETYPE_SUPPORT) {
                bool lowHp = false;
                for (int ah = 0; ah < trainers_[i].heroCount(); ah++) {
                    if (trainers_[i].heroAt(ah).alive() && (float)trainers_[i].heroAt(ah).hp() / trainers_[i].heroAt(ah).maxHp() < 0.4f) {
                        lowHp = true; break;
                    }
                }
                static_cast<HeroSupport&>(hero).anyAllyLowHp_ = lowHp;
            } else if (hero.archetype() == ARCHETYPE_ASSASSIN) {
                float minPct = 1.0f;
                for (int eh = 0; eh < trainers_[1-i].heroCount(); eh++) {
                    if (trainers_[1-i].heroAt(eh).alive()) {
                        minPct = std::min(minPct, (float)trainers_[1-i].heroAt(eh).hp() / trainers_[1-i].heroAt(eh).maxHp());
                    }
                }
                static_cast<HeroAssassin&>(hero).lowestEnemyHpPct_ = minPct;
            }

            if (hero.shouldTriggerUltimate()) {
                // Probabilistic gate
                if ((float)rand() / RAND_MAX < hero.ultimateProbabilityPerTick()) {
                    // Gather lists for activateUltimate
                    Hero* allies[MAX_HEROES_SIDE];
                    int allyCount = 0;
                    for (int ah = 0; ah < trainers_[i].heroCount(); ah++) allies[allyCount++] = &trainers_[i].heroAt(ah);

                    Hero* enemies[MAX_HEROES_SIDE];
                    int enemyCount = 0;
                    for (int eh = 0; eh < trainers_[1 - i].heroCount(); eh++) enemies[enemyCount++] = &trainers_[1 - i].heroAt(eh);

                    hero.activateUltimate(allies, allyCount, enemies, enemyCount);
                }
            }
        }
    }
}

void Game::resolveTimeLimit() { /* Similar to MVP, check total team HP % */ }

void Game::generateBuffZones() { /* Same as original logic */ }

bool Game::isConnected(int pid) const { return trainers_[pid].isConnected(); }
bool Game::playerMatchesAddr(int pid, const sockaddr_in& a) const { return trainers_[pid].matchesAddr(a); }
const sockaddr_in& Game::playerAddr(int pid) const { return trainers_[pid].addr(); }

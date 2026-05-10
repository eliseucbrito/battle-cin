#include "../../include/game.h"
#include "../../include/hero_assassin.h"
#include "../../include/hero_support.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

Game::Game()
    : connectedCount_(0), phase_(PHASE_WAITING), phaseTimer_(0.f),
      buffZoneCount_(0), roundWinner_(0xFF), matchWinner_(0xFF),
      initialized_(false)
{
    selected_[0] = false;
    selected_[1] = false;
}

void Game::registerPlayer(int pid, const sockaddr_in& from)
{
    if (pid < 0 || pid > 1) return;
    trainers_[pid].setAddr(from);
    trainers_[pid].resetScore();

    ++connectedCount_;
    printf("Player %d conectado! (%d/2)\n", pid, connectedCount_);

    // Only start if both connected AND both selected AND not yet initialized
    if (connectedCount_ == 2 && selected_[0] && selected_[1] && !initialized_)
        initFromSelections();
}

void Game::handleSelect(int pid, const SelectionPacket& sel)
{
    if (pid < 0 || pid > 1) return;
    if (selected_[pid]) return;  // already selected

    selections_[pid] = sel;
    selected_[pid] = true;
    printf("Player %d selected trainer %d with heroes [%d, %d, %d]\n",
           pid, sel.trainerIndex, sel.heroIndices[0], sel.heroIndices[1], sel.heroIndices[2]);

    if (connectedCount_ == 2 && selected_[0] && selected_[1] && !initialized_)
        initFromSelections();
}

void Game::initFromSelections()
{
    // Save addresses before reassigning trainers
    sockaddr_in savedAddrs[2] = { trainers_[0].addr(), trainers_[1].addr() };

    for (int i = 0; i < 2; i++) {
        const SelectionPacket& sel = selections_[i];
        const TrainerDefEntry& tDef = TRAINER_DEFS[sel.trainerIndex];
        trainers_[i] = Trainer(tDef.name, tDef.discipline, i, sel.trainerIndex, tDef.abilityType);
        trainers_[i].setAddr(savedAddrs[i]);  // restore address

        for (int h = 0; h < 3; h++) {
            const HeroDefEntry& hDef = HERO_DEFS[sel.heroIndices[h]];
            trainers_[i].addHero(hDef.archetype, hDef.hp, hDef.ad, hDef.arm, sel.heroIndices[h]);
        }
    }

    initialized_ = true;
    phase_ = PHASE_VS_INTRO;
    phaseTimer_ = VS_INTRO_TIME;
    printf("Game initialized from player selections! VS intro...\n");
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

void Game::handleTarget(int pid, int heroIdx, int targetIdx)
{
    if (phase_ != PHASE_BATTLE) return;
    if (heroIdx < 0 || heroIdx >= trainers_[pid].heroCount()) return;

    Hero& hero = trainers_[pid].heroAt(heroIdx);
    if (!hero.alive()) return;

    if (targetIdx < 0) {
        hero.clearTargetFocus();
        return;
    }

    if (targetIdx >= trainers_[1 - pid].heroCount()) return;
    Hero& target = trainers_[1 - pid].heroAt(targetIdx);
    if (!target.alive()) return;

    // Only allow targeting adjacent enemies
    if (!hero.isAdjacentTo(target)) return;

    hero.setTargetFocus(targetIdx);
}

void Game::update(float dt)
{
    for (int i = 0; i < 2; i++) {
        for (int h = 0; h < trainers_[i].heroCount(); h++) {
            Hero& hero = trainers_[i].heroAt(h);
            hero.tickTimers(dt);
            hero.tickUltimate(dt);
            hero.tickEffects(dt);
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

        case PHASE_VS_INTRO:
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
                    hero.archetype(), hero.heroDefIndex(), hero.buff(),
                    (uint8_t)hero.alive(), (uint8_t)hero.ultActive(), (uint8_t)i,
                    hero.targetFocus()
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
                    hero.applyBuff(buffZones_[b].type, 30.f);  // 30s duration
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

            Hero* target = nullptr;

            // Try focused target first
            if (hero.targetFocus() >= 0) {
                int tf = hero.targetFocus();
                if (tf < trainers_[1 - i].heroCount()) {
                    Hero& focused = trainers_[1 - i].heroAt(tf);
                    if (focused.alive() && hero.isAdjacentTo(focused))
                        target = &focused;
                    else
                        hero.clearTargetFocus();
                }
            }

            // Fallback: first adjacent enemy
            if (!target) {
                for (int eh = 0; eh < trainers_[1 - i].heroCount(); eh++) {
                    Hero& enemy = trainers_[1 - i].heroAt(eh);
                    if (enemy.alive() && hero.isAdjacentTo(enemy)) {
                        target = &enemy;
                        break;
                    }
                }
            }

            if (target) hero.attackTarget(*target);
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

void Game::resolveTimeLimit() {
    float hpPct[2] = { 0.f, 0.f };
    for (int i = 0; i < 2; i++) {
        int totalHp = 0, totalMaxHp = 0;
        for (int h = 0; h < trainers_[i].heroCount(); h++) {
            const Hero& hero = trainers_[i].heroAt(h);
            totalHp    += hero.hp();
            totalMaxHp += hero.maxHp();
        }
        hpPct[i] = (totalMaxHp > 0) ? (float)totalHp / totalMaxHp : 0.f;
    }
    if      (hpPct[0] > hpPct[1]) endRound(0);
    else if (hpPct[1] > hpPct[0]) endRound(1);
    else                           endRound(0xFF);  // empate
}

void Game::generateBuffZones() {
    buffZoneCount_ = 0;
    int desired = 2 + (rand() % 2);  // 2 ou 3 zonas
    for (int attempt = 0; attempt < 20 && buffZoneCount_ < desired; attempt++) {
        uint8_t bx = (uint8_t)(2 + rand() % 4);   // cols 2-5
        uint8_t by = (uint8_t)(rand() % GRID_ROWS);
        bool dup = false;
        for (int i = 0; i < buffZoneCount_; i++) {
            if (buffZones_[i].x == bx && buffZones_[i].y == by) { dup = true; break; }
        }
        if (dup) continue;
        uint8_t type = (uint8_t)(1 + rand() % 3);  // BUFF_AD, BUFF_HP ou BUFF_ARM
        buffZones_[buffZoneCount_++] = { bx, by, type };
    }
}

bool Game::isConnected(int pid) const { return trainers_[pid].isConnected(); }
bool Game::playerMatchesAddr(int pid, const sockaddr_in& a) const { return trainers_[pid].matchesAddr(a); }
const sockaddr_in& Game::playerAddr(int pid) const { return trainers_[pid].addr(); }

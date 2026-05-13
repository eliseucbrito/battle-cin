#include "../../include/game.h"
#include "../../include/hero_assassin.h"
#include "../../include/hero_support.h"
#include "../../include/priority_queue.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cmath>

std::vector<TrainerDefEntry> g_trainerDefs;
std::vector<HeroDefEntry>    g_heroDefs;

struct Combatant {
    Hero* hero;
    int   team;
    float asRate;
    bool operator>(const Combatant& other) const { return asRate > other.asRate; }
};

Game::Game()
    : connectedCount_(0), phase_(PHASE_SELECT), phaseTimer_(0.f),
      buffZoneCount_(0), roundWinner_(0xFF), matchWinner_(0xFF),
      roundNumber_(0), initialized_(false), tournament_(WIN_SCORE),
      selectSubphase_(0), selectTimer_(SELECT_TRAINER_TIME),
      db_(), shop_(db_.getAllShopItems())
{
    selected_[0] = false;
    selected_[1] = false;
    isBot_[0] = false;
    isBot_[1] = false;
    botPlaced_ = false;
    trainerLocked_[0] = false;
    trainerLocked_[1] = false;
    trainerChoice_[0] = 0;
    trainerChoice_[1] = 0;
    herosLocked_[0] = false;
    herosLocked_[1] = false;
    for (int p = 0; p < 2; p++)
        for (int h = 0; h < 3; h++)
            heroChoices_[p][h] = 0;
    loadDefs();
}

void Game::loadDefs()
{
    g_trainerDefs.clear();
    auto trainers = db_.getAllTrainers();
    for (const auto& t : trainers) {
        TrainerDefEntry e;
        e.name = t.name;
        e.discipline = t.discipline;
        e.abilityType = (uint8_t)t.ability_type;
        e.abilityName = t.ability_name;
        e.abilityDesc = t.ability_desc;
        e.colorR = (uint8_t)t.color_r;
        e.colorG = (uint8_t)t.color_g;
        e.colorB = (uint8_t)t.color_b;
        e.portraitPath = t.portrait_path;
        e.cardPath = t.card_path;
        g_trainerDefs.push_back(std::move(e));
    }

    g_heroDefs.clear();
    auto heroes = db_.getAllHeroes();
    for (const auto& h : heroes) {
        HeroDefEntry e;
        e.name = h.name;
        e.monologue = h.monologue;
        e.description = h.description;
        e.archetype = (uint8_t)h.archetype;
        e.trainerId = (uint8_t)h.trainer_id;
        e.className = h.class_name;
        e.hp = h.hp;
        e.ad = h.ad;
        e.arm = h.arm;
        e.assetPath = h.asset_path;
        e.ultimateNames.push_back(h.ultimate_name_1);
        e.ultimateNames.push_back(h.ultimate_name_2);
        e.ultimateNames.push_back(h.ultimate_name_3);
        e.dyingPhrase = h.dying_phrase;
        g_heroDefs.push_back(std::move(e));
    }
    printf("[Game] %zu treinadores e %zu herois carregados do banco.\n",
           g_trainerDefs.size(), g_heroDefs.size());
}

void Game::registerPlayerLocal(int pid)
{
    if (pid < 0 || pid > 1) return;
    trainers_[pid].setConnected(true);
    trainers_[pid].resetScore();
    ++connectedCount_;
    printf("Player %d registrado localmente!\n", pid);
}

void Game::handleLocalTrainerLock(int pid, uint8_t trainerIdx)
{
    if (pid < 0 || pid > 1) return;
    if (trainerLocked_[pid]) return;
    trainerLocked_[pid] = true;
    trainerChoice_[pid] = trainerIdx;
    printf("Player %d locked trainer %d\n", pid, trainerIdx);
}

void Game::handleLocalHeroPick(int pid, const uint8_t heroIndices[3])
{
    if (pid < 0 || pid > 1) return;
    if (herosLocked_[pid]) return;
    for (int h = 0; h < 3; h++)
        heroChoices_[pid][h] = heroIndices[h];
    herosLocked_[pid] = true;
    printf("Player %d locked heroes [%d,%d,%d]\n",
           pid, heroIndices[0], heroIndices[1], heroIndices[2]);
}

void Game::initFromSelections()
{
    for (int i = 0; i < 2; i++) {
        uint8_t tIdx = trainerChoice_[i];
        const TrainerDefEntry& tDef = g_trainerDefs[tIdx];
        trainers_[i] = Trainer(tDef.name, tDef.discipline, i, tIdx, tDef.abilityType);

        for (int h = 0; h < 3; h++) {
            uint8_t hIdx = heroChoices_[i][h];
            const HeroDefEntry& hDef = g_heroDefs[hIdx];
            trainers_[i].addHero(hDef.archetype, hDef.hp, hDef.ad, hDef.arm, hIdx);
        }
    }

    initialized_ = true;
    phase_ = PHASE_VS_INTRO;
    phaseTimer_ = VS_INTRO_TIME;
    printf("Game initialized from player selections! VS intro...\n");
}

void Game::createBot(int pid)
{
    if (pid < 0 || pid > 1) return;
    if (trainerLocked_[pid]) return;

    int tIdx = rand() % (int)g_trainerDefs.size();
    int hIdx[3];
    std::vector<int> available;
    for (int i = 0; i < (int)g_heroDefs.size(); i++)
        if (g_heroDefs[i].trainerId == (uint8_t)(tIdx + 1))
            available.push_back(i);
    for (int s = (int)available.size() - 1; s > 0; s--) {
        int r = rand() % (s + 1);
        std::swap(available[s], available[r]);
    }
    for (int i = 0; i < 3 && i < (int)available.size(); i++)
        hIdx[i] = available[i];

    trainerLocked_[pid] = true;
    trainerChoice_[pid] = (uint8_t)tIdx;
    for (int i = 0; i < 3; i++)
        heroChoices_[pid][i] = (uint8_t)hIdx[i];
    herosLocked_[pid] = true;
    isBot_[pid] = true;

    printf("Bot created for Player %d (trainer=%d, heroes=%d,%d,%d)\n",
           pid, tIdx, hIdx[0], hIdx[1], hIdx[2]);
}

void Game::updateBot(float dt)
{
    (void)dt;
    for (int pid = 0; pid < 2; pid++) {
        if (!isBot_[pid]) continue;

        if (phase_ == PHASE_POSITIONING) {
            if (!botPlaced_) {
                bool isLeft = (pid == 0);
                for (int h = 0; h < trainers_[pid].heroCount(); h++) {
                    uint8_t arch = trainers_[pid].heroAt(h).archetype();
                    uint8_t tx, ty;
                    if (arch == ARCHETYPE_TANK)       { tx = isLeft ? 3 : 4; ty = 2 + h; }
                    else if (arch == ARCHETYPE_MAGE)  { tx = isLeft ? 0 : 7; ty = 2 + h; }
                    else if (arch == ARCHETYPE_SUPPORT){ tx = isLeft ? 1 : 6; ty = 2 + h; }
                    else                              { tx = isLeft ? 2 : 5; ty = 2 + h; }
                    trainers_[pid].placeHero(h, tx, ty, isLeft);
                }
                botPlaced_ = true;
            }
        } else {
            botPlaced_ = false;
        }

        if (phase_ == PHASE_BATTLE) {
            if (trainers_[pid].canUseAbility()) {
                trainers_[pid].useAbility();
            }
        }

        if (phase_ == PHASE_SHOP) {
            shop_.botShop(pid, trainers_[pid], dt);
        }
    }
}

bool Game::handlePlaceHero(int pid, int heroIdx, uint8_t tx, uint8_t ty)
{
    if (phase_ != PHASE_POSITIONING) return false;
    if (tx >= GRID_COLS || ty >= GRID_ROWS) return false;

    bool isLeft = (pid == 0);
    return trainers_[pid].placeHero(heroIdx, tx, ty, isLeft);
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

    if (!hero.isAdjacentTo(target)) return;

    hero.setTargetFocus(targetIdx);
}

void Game::autoPickHeroes(int pid)
{
    if (herosLocked_[pid]) return;
    uint8_t tIdx = trainerChoice_[pid];
    std::vector<int> available;
    for (int i = 0; i < (int)g_heroDefs.size(); i++)
        if (g_heroDefs[i].trainerId == (uint8_t)(tIdx + 1))
            available.push_back(i);
    for (int s = (int)available.size() - 1; s > 0; s--) {
        int r = rand() % (s + 1);
        std::swap(available[s], available[r]);
    }
    for (int h = 0; h < 3 && h < (int)available.size(); h++)
        heroChoices_[pid][h] = (uint8_t)available[h];
    herosLocked_[pid] = true;
}

void Game::update(float dt)
{
    updateBot(dt);

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
        case PHASE_SELECT:
            selectTimer_ -= dt;
            if (selectSubphase_ == 0) {
                if (selectTimer_ <= 0.f || (trainerLocked_[0] && trainerLocked_[1])) {
                    for (int i = 0; i < 2; i++)
                        if (!trainerLocked_[i]) trainerChoice_[i] = rand() % (int)g_trainerDefs.size();
                    selectSubphase_ = 1;
                    selectTimer_ = SELECT_HERO_TIME;
                }
            } else {
                if (selectTimer_ <= 0.f || (herosLocked_[0] && herosLocked_[1])) {
                    for (int i = 0; i < 2; i++)
                        if (!herosLocked_[i]) autoPickHeroes(i);
                    initFromSelections();
                }
            }
            break;

        case PHASE_POSITIONING:
            phaseTimer_ -= dt;
            if (phaseTimer_ <= 0.f) startBattle();
            break;

        case PHASE_BATTLE:
            phaseTimer_ -= dt;
            autoBattleMove();
            
            // --- Auto-use Health Potions ---
            for (int i = 0; i < 2; i++) {
                for (int h = 0; h < trainers_[i].heroCount(); h++) {
                    Hero& hero = trainers_[i].heroAt(h);
                    if (!hero.alive()) continue;
                    if ((float)hero.hp() / hero.maxHp() < 0.5f) {
                        for (int s = 0; s < MAX_HERO_ITEMS; s++) {
                            uint8_t itemId = hero.itemInSlot(s);
                            // itemId % 5 == 0 is Health Potion according to database.cpp seed
                            if (itemId != 0xFF && (itemId % 5 == 0)) {
                                const Item* proto = shop_.getPrototype(itemId);
                                if (proto && proto->use(hero, roundNumber_)) {
                                    hero.unequipItem(s);
                                    printf("[AutoUse] P%d hero %d usou %s\n", i, h, proto->name().c_str());
                                    break; 
                                }
                            }
                        }
                    }
                }
            }

            runCombat();
            tickUltimates(dt);
            
            if (!trainers_[0].hasLiveHeroes()) endRound(1);
            else if (!trainers_[1].hasLiveHeroes()) endRound(0);
            else if (phaseTimer_ <= 0.f) resolveTimeLimit();
            break;

        case PHASE_ROUND_END:
            phaseTimer_ -= dt;
            if (phaseTimer_ <= 0.f) {
                if (matchWinner_ == 0xFF) {
                    enterShopPhase();
                }
            }
            break;

        case PHASE_SHOP:
            updateBot(dt);
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

    snap.selectSubphase = selectSubphase_;
    snap.selectTimer = selectTimer_;
    for (int i = 0; i < 2; i++) {
        snap.trainerLocked[i] = trainerLocked_[i] ? 1 : 0;
        snap.trainerChoice[i] = trainerChoice_[i];
        snap.herosLocked[i] = herosLocked_[i] ? 1 : 0;
        for (int h = 0; h < 3; h++)
            snap.heroPicks[i][h] = heroChoices_[i][h];
    }

    if (!initialized_) {
        snap.heroCount = 0;
        for (int i = 0; i < 2; i++)
            snap.trainers[i] = {0, 0, 0, 0, 0, 0, {0xFF, 0xFF, 0xFF}};
        snap.buffZoneCount = 0;
        for (int i = 0; i < 4; i++)
            snap.buffZones[i] = {0xFF, 0xFF, BUFF_NONE};
        return;
    }

    int totalHeroes = 0;
    for (int i = 0; i < 2; i++) {
        const Trainer& t = trainers_[i];
        snap.trainers[i] = {
            t.trainerId(), t.score(), (uint8_t)t.heroCount(), (uint8_t)t.canUseAbility(),
            (uint16_t)(shop_.isOpen() ? shop_.gold(i) : 0),
            t.generalItemCount(),
            { t.generalItemInSlot(0), t.generalItemInSlot(1), t.generalItemInSlot(2) }
        };

        for (int h = 0; h < t.heroCount(); h++) {
            const Hero& hero = t.heroAt(h);
            if (totalHeroes < MAX_HEROES_TOTAL) {
                snap.heroes[totalHeroes++] = {
                    hero.x(), hero.y(),
                    (uint16_t)hero.hp(), (uint16_t)hero.maxHp(),
                    (uint8_t)hero.ad(), (uint8_t)hero.arm(),
                    (uint8_t)(hero.asRate() * 10.f + 0.5f),
                    hero.archetype(), hero.heroDefIndex(), hero.buff(),
                    (uint8_t)hero.alive(), (uint8_t)hero.ultActive(),
                    [&]() -> uint8_t {
                        if (hero.ultActive()) return 255;
                        if (hero.ultReady())  return 100;
                        float pct = 1.0f - hero.ultCooldownTimer() / hero.ultimateCooldown();
                        if (pct < 0.f) pct = 0.f;
                        return (uint8_t)(pct * 100.f);
                    }(),
                    hero.ultPhraseIdx(),
                    (uint8_t)i,
                    hero.targetFocus(),
                    hero.itemCount(),
                    { hero.itemInSlot(0), hero.itemInSlot(1),
                      hero.itemInSlot(2), hero.itemInSlot(3) }
                };
            }
        }
    }
    snap.heroCount = (uint8_t)totalHeroes;

    shop_.buildShopSnapshot(snap.shop);

    for (int i = 0; i < 4; i++) {
        snap.buffZones[i] = (i < buffZoneCount_) ? buffZones_[i] : BuffZoneInfo{0xFF, 0xFF, BUFF_NONE};
    }
}

void Game::startPositioning()
{
    phase_ = PHASE_POSITIONING;
    phaseTimer_ = POSITIONING_TIME;
    roundWinner_ = 0xFF;
    ++roundNumber_;

    for (int i = 0; i < 2; i++) {
        trainers_[i].resetForRound();
        for (int h = 0; h < trainers_[i].heroCount(); h++) {
            Hero& hero = trainers_[i].heroAt(h);
            hero.tickTempItems(roundNumber_);
            hero.setPosition(i == 0 ? 1 : 6, 2 + h);
        }
    }
    generateBuffZones();
    botPlaced_ = false;
}

void Game::startBattle()
{
    phase_ = PHASE_BATTLE;
    phaseTimer_ = BATTLE_MAX_TIME;

    for (int i = 0; i < 2; i++) {
        for (int h = 0; h < trainers_[i].heroCount(); h++) {
            Hero& hero = trainers_[i].heroAt(h);
            for (int b = 0; b < buffZoneCount_; b++) {
                if (hero.x() == buffZones_[b].x && hero.y() == buffZones_[b].y) {
                    hero.applyBuff(buffZones_[b].type, 30.f);
                    break;
                }
            }
        }
    }
}

void Game::endRound(uint8_t winner)
{
    roundWinner_ = winner;
    tournament_.recordRoundResult(roundNumber_, winner);
    if (winner != 0xFF) {
        trainers_[winner].addScore();
        uint8_t loser = 1 - winner;
        db_.saveMatch(
            trainers_[winner].name(),
            trainers_[loser].name(),
            trainers_[winner].score(),
            trainers_[loser].score()
        );

        uint8_t tw = tournament_.getMatchWinner();
        if (tw != 0xFF) {
            matchWinner_ = tw;
            phase_ = PHASE_MATCH_END;

            printf("Match winner: Player %d!\n", tw);
            tournament_.print();

            printf("\n=== RANKING ===\n");
            auto ranking = db_.getRanking();
            for (const auto& r : ranking) {
                printf("  %s — %d vitorias / %d derrotas\n",
                       r.name.c_str(), r.wins, r.losses);
            }
            printf("===============\n\n");
            return;
        }
    }
    phase_ = PHASE_ROUND_END;
    phaseTimer_ = ROUND_END_TIME;
}

void Game::enterShopPhase() {
    phase_ = PHASE_SHOP;
    phaseTimer_ = 9999.f;
    shop_.enterShopPhase(roundNumber_ + 1, trainers_[0], trainers_[1]);
    botPlaced_ = false;
}

void Game::handleBuyItem(int pid, int stockIdx, int heroIdx, int slotIdx) {
    if (phase_ != PHASE_SHOP) return;
    shop_.buy(pid, stockIdx, trainers_[pid], heroIdx, slotIdx);
}

void Game::handleRefreshShop(int pid) {
    if (phase_ != PHASE_SHOP) return;
    shop_.refreshStock(pid);
}

void Game::handleConfirmShop(int pid) {
    if (!shop_.isOpen()) return;
    if (shop_.confirm(pid)) {
        startPositioning();
    }
}

void Game::handleUseGeneralItem(int pid, int slot) {
    if (phase_ != PHASE_BATTLE) return;
    uint8_t itemId = trainers_[pid].generalItemInSlot(slot);
    if (itemId == 0xFF) return;

    const Item* proto = shop_.getPrototype(itemId);
    if (!proto) return;

    if (itemId == ITEM_ID_GOLD_RUSH) {
        shop_.addBonusGold(pid, 50);
        printf("[GeneralItem] P%d usou Corrida do Ouro — +50g no proximo round\n", pid);
        trainers_[pid].removeGeneralItem(slot);
        return;
    }

    for (int h = 0; h < trainers_[pid].heroCount(); h++) {
        Hero& hero = trainers_[pid].heroAt(h);
        if (hero.alive()) {
            proto->apply(hero, roundNumber_);
        }
    }
    printf("[GeneralItem] P%d usou %s\n", pid, proto->name().c_str());
    trainers_[pid].removeGeneralItem(slot);
}

void Game::autoBattleMove()
{
    for (int i = 0; i < 2; i++) {
        bool blocked[GRID_ROWS][GRID_COLS];
        for (int y = 0; y < GRID_ROWS; y++)
            for (int x = 0; x < GRID_COLS; x++)
                blocked[y][x] = false;

        for (int t = 0; t < 2; t++) {
            for (int h = 0; h < trainers_[t].heroCount(); h++) {
                Hero& hero = trainers_[t].heroAt(h);
                if (hero.alive()) blocked[hero.y()][hero.x()] = true;
            }
        }

        for (int h = 0; h < trainers_[i].heroCount(); h++) {
            Hero& hero = trainers_[i].heroAt(h);
            if (!hero.alive() || hero.moveTimer() > 0.f) continue;

            Hero* target = nullptr;
            if (hero.targetFocus() >= 0) {
                int tf = hero.targetFocus();
                if (tf < trainers_[1 - i].heroCount()) {
                    Hero& focused = trainers_[1 - i].heroAt(tf);
                    if (focused.alive()) target = &focused;
                }
            }
            if (!target) {
                float minDist = 1000.f;
                for (int eh = 0; eh < trainers_[1 - i].heroCount(); eh++) {
                    Hero& enemy = trainers_[1 - i].heroAt(eh);
                    if (!enemy.alive()) continue;
                    float d = sqrtf(powf((float)hero.x() - enemy.x(), 2) + powf((float)hero.y() - enemy.y(), 2));
                    if (d < minDist) { minDist = d; target = &enemy; }
                }
            }

            if (target && !hero.isAdjacentTo(*target)) {
                uint8_t oldX = hero.x();
                uint8_t oldY = hero.y();

                blocked[oldY][oldX] = false;
                blocked[target->y()][target->x()] = false;

                int nx, ny;
                if (graph_.findPath(hero.x(), hero.y(), target->x(), target->y(), blocked, nx, ny)) {
                    hero.setPosition(nx, ny);
                    hero.startMoveTimer();
                    blocked[oldY][oldX] = false;
                    blocked[ny][nx] = true;
                } else {
                    blocked[oldY][oldX] = true;
                }

                blocked[target->y()][target->x()] = true;
            }
        }
    }
}

void Game::runCombat()
{
    Combatant combatants[MAX_HEROES_TOTAL];
    int combatantCount = 0;

    for (int i = 0; i < 2; i++) {
        for (int h = 0; h < trainers_[i].heroCount(); h++) {
            Hero& hero = trainers_[i].heroAt(h);
            if (hero.alive() && hero.attackTimer() <= 0.f) {
                combatants[combatantCount++] = { &hero, i, hero.asRate() };
            }
        }
    }

    if (combatantCount == 0) return;

    PriorityQueue<Combatant> pq;

    for (int i = 0; i < combatantCount; i++) {
        pq.push(combatants[i]);
    }

    while (!pq.empty()) {
        Combatant c = pq.pop();
        Hero& hero = *c.hero;
        if (!hero.alive()) continue;
        int i = c.team;

        Hero* target = nullptr;

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

void Game::tickUltimates(float dt)
{
    for (int i = 0; i < 2; i++) {
        for (int h = 0; h < trainers_[i].heroCount(); h++) {
            Hero& hero = trainers_[i].heroAt(h);
            if (!hero.alive() || !hero.ultReady()) continue;

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
                if ((float)rand() / RAND_MAX < hero.ultimateProbabilityPerTick()) {
                    Hero* allies[MAX_HEROES_SIDE];
                    int allyCount = 0;
                    for (int ah = 0; ah < trainers_[i].heroCount(); ah++) allies[allyCount++] = &trainers_[i].heroAt(ah);

                    Hero* enemies[MAX_HEROES_SIDE];
                    int enemyCount = 0;
                    for (int eh = 0; eh < trainers_[1 - i].heroCount(); eh++) enemies[enemyCount++] = &trainers_[1 - i].heroAt(eh);

                    hero.setUltPhraseIdx((hero.ultPhraseIdx() + 1) % 3);
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
    else                           endRound(0xFF);
}

void Game::generateBuffZones() {
    buffZoneCount_ = 0;
    int desired = 2 + (rand() % 2);
    for (int attempt = 0; attempt < 20 && buffZoneCount_ < desired; attempt++) {
        uint8_t bx = (uint8_t)(2 + rand() % 4);
        uint8_t by = (uint8_t)(rand() % GRID_ROWS);
        bool dup = false;
        for (int i = 0; i < buffZoneCount_; i++) {
            if (buffZones_[i].x == bx && buffZones_[i].y == by) { dup = true; break; }
        }
        if (dup) continue;
        uint8_t type = (uint8_t)(1 + rand() % 3);
        buffZones_[buffZoneCount_++] = { bx, by, type };
    }
}

void Game::handleDebugReduceHP(int pid) {
    if (pid < 0 || pid > 1) return;
    for (int h = 0; h < trainers_[pid].heroCount(); h++) {
        Hero& hero = trainers_[pid].heroAt(h);
        if (hero.alive()) {
            hero.healHp(-(hero.hp() - 1));
        }
    }
    printf("[DEBUG] Reduzida vida dos herois do P%d para 1\n", pid + 1);
}

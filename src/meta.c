/* GOUFFRE - progression : defis, deblocages, codex, defi du jour */
#include "game.h"
#include <time.h>

enum { ST_KILLS, ST_RUNS, ST_WINS, ST_LEVEL, ST_BOSSES, ST_BOSS0, ST_BOSS1, ST_BOSS2, ST_BOSS3, ST_ELITES,
       ST_EVOS, ST_EVO_DISC, ST_COMBO, ST_PLEVEL, ST_GOLD, ST_DEPTH, ST_HERO_WINS, ST_HEROES, ST_BLD,
       ST_EVENTS, ST_ENDLESS, ST_DAILY, ST_DAILY_WIN, ST_CODEX, ST_FEAT,
       /* v4 */
       ST_BOSSN, ST_DEPTH2, ST_DWON, ST_PROF_WINS, ST_PROF_RUNS, ST_RANKS, ST_RANK, ST_STARS, ST_STAR3, ST_BEST,
       ST_BLDMAX, ST_CHAMPS, ST_BOSSDISC, ST_BOSSMIN, ST_COLL,
       /* v6 */
       ST_LVLS, ST_LVLHEROES, ST_CHECKS, ST_MBOSS,
       /* v7 : le Royaume oublie */
       ST_DEPTH3, ST_ROY_WINS, ST_ROY_RUNS, ST_KILLKIND, ST_EVOK, ST_ROYLVLS, ST_BLDLV, ST_CODEX7,
       /* v8 : curiosites du decor */
       ST_CURIO, ST_CURIOK,
       /* v9 */
       ST_LEG, ST_LEGALL, ST_OBJ, ST_RUSH, ST_RUSHDEST, ST_MARKK,
       /* v10 */
       ST_MUTWIN, ST_MUTMAX, ST_CHAMPK, ST_CHAMPALL, ST_QUAKEK, ST_LINKK };

/* l'ordre ne doit jamais changer (indices sauvegardes) */
const AchDef ACHS[] = {
  {"Premiers pas","Terminer une descente.",ST_RUNS,0,1,RW_GOLD,50},
  {"Chasseur","Vaincre 250 monstres.",ST_KILLS,0,250,RW_BALL,BK_SANG},
  {"Les Catacombes","Vaincre le Roi des Os.",ST_BOSS0,0,1,RW_PAS,PS_BOUCLIER},
  {"Habitué","Terminer 3 descentes.",ST_RUNS,0,3,RW_MODE,MODE_DAILY},
  {"Fusion","Réaliser une évolution.",ST_EVOS,0,1,RW_BALL,BK_CRISTAL},
  {"Collectionneur","Avoir 5 billes en même temps.",ST_FEAT,FEAT_FULL_BALLS,1,RW_BALL,BK_ECLAT},
  {"La Forêt Fongique","Vaincre la Mère-Spore.",ST_BOSS1,0,1,RW_BALL,BK_RAFALE},
  {"Intouchable","Vaincre un gardien sans avoir subi de dégâts depuis le précédent.",ST_FEAT,FEAT_NOHIT_LEVEL,1,RW_BALL,BK_LUMIERE},
  {"Série","Atteindre un combo de 100.",ST_COMBO,0,100,RW_BALL,BK_FOLLE},
  {"Élites","Vaincre 25 élites.",ST_ELITES,0,25,RW_BALL,BK_PLOMB},
  {"Mains d'or","Rapporter 2 000 or au total.",ST_GOLD,0,2000,RW_PAS,PS_TREFLE},
  {"Les Forges","Vaincre le Colosse de Magma.",ST_BOSS2,0,1,RW_BALL,BK_AIMANT},
  {"Endurant","Terminer 2 descentes.",ST_RUNS,0,2,RW_PAS,PS_MOUSSE},
  {"Purificateur","Vaincre l'Œil du Gouffre.",ST_BOSSN,BOSS_OEIL,1,RW_MODE,MODE_ENDLESS},
  {"Grand héros","Atteindre le niveau 25 de héros.",ST_PLEVEL,0,25,RW_PAS,PS_MIROIR},
  {"Sang-froid","Vaincre un gardien avec moins de 10% de PV.",ST_FEAT,FEAT_LOW_HP_BOSS,1,RW_PAS,PS_RAGE},
  {"Voyageur","Vivre 5 événements dans le gouffre.",ST_EVENTS,0,5,RW_PAS,PS_FIOLE},
  {"Pêcheur","Débloquer le niveau 2.",ST_LEVEL,0,2,RW_PAS,PS_FILET},
  {"Âmes errantes","Vaincre 1 250 monstres.",ST_KILLS,0,1250,RW_HERO,CH_NECRO},
  {"Contre la montre","Vaincre un gardien (sauf le premier) en moins de 40 s.",ST_FEAT,FEAT_FAST_BOSS,1,RW_HERO,CH_HORLOGERE},
  {"Pyrolâtre","Vaincre 10 gardiens.",ST_BOSSES,0,10,RW_HERO,CH_PYROMANE},
  {"Massacre","Vaincre 5 000 monstres.",ST_KILLS,0,5000,RW_CRYSTAL,3},
  {"Légion","Vaincre 15 000 monstres.",ST_KILLS,0,15000,RW_CRYSTAL,5},
  {"Vétéran","Terminer 10 descentes.",ST_RUNS,0,10,RW_GOLD,300},
  {"Pilier du campement","Terminer 30 descentes.",ST_RUNS,0,30,RW_CRYSTAL,3},
  {"Triomphe","Terminer 5 niveaux.",ST_WINS,0,5,RW_CRYSTAL,3},
  {"Légende","Terminer 15 niveaux.",ST_WINS,0,15,RW_CRYSTAL,5},
  {"Alchimie","Découvrir 6 évolutions.",ST_EVO_DISC,0,6,RW_GOLD,300},
  {"Grand œuvre","Découvrir les 24 évolutions.",ST_EVO_DISC,0,EVO_COUNT,RW_CRYSTAL,5},
  {"Trois fusions","Réaliser 3 évolutions en une descente.",ST_FEAT,FEAT_3EVOS,1,RW_GOLD,250},
  {"Combo infernal","Atteindre un combo de 300.",ST_COMBO,0,300,RW_CRYSTAL,2},
  {"Fléau des élites","Vaincre 150 élites.",ST_ELITES,0,150,RW_CRYSTAL,2},
  {"Maître","Atteindre le niveau 35 de héros.",ST_PLEVEL,0,35,RW_CRYSTAL,2},
  {"Fortune","Rapporter 10 000 or au total.",ST_GOLD,0,10000,RW_CRYSTAL,2},
  {"Trésor des abysses","Rapporter 600 or en une seule descente.",ST_FEAT,FEAT_RICH,1,RW_GOLD,400},
  {"Profondeur 2","Débloquer la profondeur 2 du Cœur du Gouffre.",ST_DEPTH,0,2,RW_GOLD,200},
  {"Profondeur 3","Débloquer la profondeur 3 du Cœur du Gouffre.",ST_DEPTH,0,3,RW_CRYSTAL,2},
  {"Abysses","Débloquer la profondeur 5 du Cœur du Gouffre.",ST_DEPTH,0,5,RW_CRYSTAL,5},
  {"Polyvalent","Terminer un niveau avec 3 héros différents.",ST_HERO_WINS,0,3,RW_CRYSTAL,2},
  {"Panthéon","Terminer un niveau avec chacun des 10 héros.",ST_HERO_WINS,0,CH_COUNT,RW_CRYSTAL,8},
  {"Bâtisseur","Atteindre 15 niveaux de bâtiments.",ST_BLD,0,15,RW_GOLD,300},
  {"Architecte","Atteindre 40 niveaux de bâtiments.",ST_BLD,0,40,RW_CRYSTAL,3},
  {"Explorateur","Vivre 25 événements dans le gouffre.",ST_EVENTS,0,25,RW_CRYSTAL,2},
  {"Sans fin I","Atteindre l'étage 6 en mode infini.",ST_ENDLESS,0,6,RW_CRYSTAL,2},
  {"Sans fin II","Atteindre l'étage 10 en mode infini.",ST_ENDLESS,0,10,RW_CRYSTAL,4},
  {"Sans fin III","Atteindre l'étage 16 en mode infini.",ST_ENDLESS,0,16,RW_CRYSTAL,8},
  {"Défi du jour","Terminer un défi du jour.",ST_DAILY,0,1,RW_GOLD,150},
  {"Assidu","Terminer 7 défis du jour.",ST_DAILY,0,7,RW_CRYSTAL,3},
  {"Héros du jour","Gagner un défi du jour.",ST_DAILY_WIN,0,1,RW_CRYSTAL,3},
  {"Bestiaire","Vaincre 15 espèces de monstres.",ST_CODEX,0,15,RW_GOLD,200},
  {"Naturaliste","Vaincre toutes les espèces de monstres.",ST_CODEX,0,EK_COUNT,RW_CRYSTAL,3},
  {"Cinq cents d'un coup","Vaincre 500 monstres en une descente.",ST_FEAT,FEAT_KILLS_1000,1,RW_GOLD,300},
  {"Éclair","Vaincre les trois gardiens d'un niveau en moins de 80 s au total.",ST_FEAT,FEAT_SPEED,1,RW_CRYSTAL,2},
  {"Puriste","Vaincre le Roi des Os sans avoir choisi de nouvelle bille.",ST_FEAT,FEAT_SOLO,1,RW_GOLD,200},
  {"Parfait","Vaincre un gardien sans subir de dégâts.",ST_FEAT,FEAT_BOSS_NOHIT,1,RW_CRYSTAL,2},
  {"Pacte tenu","Vaincre un gardien sous le Pacte des ombres.",ST_FEAT,FEAT_PACT,1,RW_GOLD,250},
  {"Recrutement","Posséder 5 héros.",ST_HEROES,0,5,RW_GOLD,300},
  {"Tueur de gardiens","Vaincre 40 gardiens.",ST_BOSSES,0,40,RW_CRYSTAL,4},
  /* ---------------- v4 : les Profondeurs et la progression longue ---------------- */
  {"Plus bas encore","Vaincre la Reine de Givre.",ST_BOSSN,BOSS_GIVRE,1,RW_CRYSTAL,3},
  {"Les eaux noires","Vaincre le Léviathan.",ST_BOSSN,BOSS_LEVIATHAN,1,RW_CRYSTAL,3},
  {"Miel amer","Vaincre la Reine-Mère.",ST_BOSSN,BOSS_RUCHE,1,RW_CRYSTAL,4},
  {"Au-delà du fond","Vaincre le Cœur du Néant.",ST_BOSSN,BOSS_NEANT,1,RW_CRYSTAL,8},
  {"Abyssal","Terminer 5 niveaux des Profondeurs.",ST_PROF_WINS,0,5,RW_CRYSTAL,5},
  {"Seigneur des abysses","Terminer 25 niveaux des Profondeurs.",ST_PROF_WINS,0,25,RW_CRYSTAL,10},
  {"Profondeur 6","Débloquer la profondeur 6 du Cœur du Gouffre.",ST_DEPTH,0,6,RW_CRYSTAL,3},
  {"Profondeur 8","Débloquer la profondeur 8 du Cœur du Gouffre.",ST_DEPTH,0,8,RW_CRYSTAL,4},
  {"Fond du Gouffre","Débloquer la profondeur 10 du Cœur du Gouffre.",ST_DEPTH,0,10,RW_CRYSTAL,6},
  {"Gouffre maîtrisé","Terminer le Cœur du Gouffre en profondeur 10.",ST_DWON,0,10,RW_CRYSTAL,12},
  {"Abysses 3","Débloquer la profondeur 3 du Néant.",ST_DEPTH2,0,3,RW_CRYSTAL,3},
  {"Abysses 5","Débloquer la profondeur 5 du Néant.",ST_DEPTH2,0,5,RW_CRYSTAL,4},
  {"Abysses 8","Débloquer la profondeur 8 du Néant.",ST_DEPTH2,0,8,RW_CRYSTAL,6},
  {"Abysses 10","Débloquer la profondeur 10 du Néant.",ST_DEPTH2,0,10,RW_CRYSTAL,8},
  {"Néant absolu","Terminer le Néant en profondeur 10.",ST_DWON,1,10,RW_CRYSTAL,20},
  {"Maudit","Terminer le Cœur du Gouffre, le Néant ou le Sanctuaire sous une malédiction (profondeur 6+).",ST_DWON,2,6,RW_CRYSTAL,3},
  {"Sous le poids des siècles","Terminer le Cœur du Gouffre, le Néant ou le Sanctuaire sous 3 malédictions (profondeur 8+).",ST_DWON,2,8,RW_CRYSTAL,5},
  {"Apprenti","Amener un héros au rang 3.",ST_RANKS,3,1,RW_GOLD,300},
  {"Compagnon","Amener un héros au rang 5.",ST_RANKS,5,1,RW_CRYSTAL,2},
  {"Maître d'armes","Amener un héros au rang 10.",ST_RANKS,10,1,RW_CRYSTAL,5},
  {"Escouade","Amener 4 héros au rang 5.",ST_RANKS,5,4,RW_CRYSTAL,4},
  {"Vétérans","Amener 8 héros au rang 5.",ST_RANKS,5,8,RW_CRYSTAL,6},
  {"Panthéon doré","Amener 8 héros au rang 10.",ST_RANKS,10,8,RW_CRYSTAL,25},
  {"Rempart éternel","Amener la Sentinelle au rang 10.",ST_RANK,CH_SENTINELLE,10,RW_CRYSTAL,3},
  {"Grand œuvre alchimique","Amener l'Alchimiste au rang 10.",ST_RANK,CH_ALCHIMISTE,10,RW_CRYSTAL,3},
  {"Virtuose absolu","Amener le Jongleur au rang 10.",ST_RANK,CH_JONGLEUR,10,RW_CRYSTAL,3},
  {"Acier trempé","Amener la Forgeronne au rang 10.",ST_RANK,CH_FORGERONNE,10,RW_CRYSTAL,3},
  {"Champ absolu","Amener le Magnétiseur au rang 10.",ST_RANK,CH_MAGNETISEUR,10,RW_CRYSTAL,3},
  {"Reine des âmes","Amener la Nécromancienne au rang 10.",ST_RANK,CH_NECRO,10,RW_CRYSTAL,3},
  {"Maîtresse du temps","Amener l'Horlogère au rang 10.",ST_RANK,CH_HORLOGERE,10,RW_CRYSTAL,3},
  {"Brasier éternel","Amener le Pyromane au rang 10.",ST_RANK,CH_PYROMANE,10,RW_CRYSTAL,3},
  {"Initié","Obtenir 10 étoiles de maîtrise de billes.",ST_STARS,0,10,RW_GOLD,400},
  {"Spécialiste","Obtenir 30 étoiles de maîtrise.",ST_STARS,0,30,RW_CRYSTAL,3},
  {"Virtuose des billes","Obtenir 60 étoiles de maîtrise.",ST_STARS,0,60,RW_CRYSTAL,6},
  {"Grand maître","Obtenir les 144 étoiles de maîtrise.",ST_STARS,0,STARS_MAX,RW_CRYSTAL,15},
  {"Signature","Maîtriser une bille (3 étoiles).",ST_STAR3,0,1,RW_CRYSTAL,2},
  {"Arsenal","Maîtriser 10 billes (3 étoiles).",ST_STAR3,0,10,RW_CRYSTAL,5},
  {"Érudit","Connaître 20 espèces au niveau 2 du bestiaire.",ST_BEST,2,20,RW_GOLD,400},
  {"Chasseur expert","Connaître 40 espèces au niveau 2 du bestiaire.",ST_BEST,2,40,RW_CRYSTAL,3},
  {"Encyclopédiste","Connaître 20 espèces au niveau 3 du bestiaire.",ST_BEST,3,20,RW_CRYSTAL,5},
  {"Bestiaire complet","Connaître toutes les espèces au niveau 3.",ST_BEST,3,EK_COUNT,RW_CRYSTAL,15},
  {"Découvreur","Vaincre les 12 gardiens.",ST_BOSSDISC,0,BOSS_COUNT,RW_CRYSTAL,4},
  {"Chasseur de gardiens","Vaincre 200 gardiens.",ST_BOSSES,0,200,RW_CRYSTAL,6},
  {"Fléau des gardiens","Vaincre 1 000 gardiens.",ST_BOSSES,0,1000,RW_CRYSTAL,10},
  {"Régicide","Vaincre chaque gardien au moins 10 fois.",ST_BOSSMIN,0,10,RW_CRYSTAL,8},
  {"Hécatombe","Vaincre 50 000 monstres.",ST_KILLS,0,50000,RW_CRYSTAL,5},
  {"Fin du monde","Vaincre 150 000 monstres.",ST_KILLS,0,150000,RW_CRYSTAL,8},
  {"Extinction","Vaincre 300 000 monstres.",ST_KILLS,0,300000,RW_CRYSTAL,12},
  {"Briseur d'élites","Vaincre 2 000 élites.",ST_ELITES,0,2000,RW_CRYSTAL,4},
  {"Tueur de champions","Vaincre 1 000 champions.",ST_CHAMPS,0,1000,RW_CRYSTAL,3},
  {"Légende des champions","Vaincre 8 000 champions.",ST_CHAMPS,0,8000,RW_CRYSTAL,6},
  {"Vieux briscard","Terminer 100 descentes.",ST_RUNS,0,100,RW_CRYSTAL,4},
  {"Inépuisable","Terminer 350 descentes.",ST_RUNS,0,350,RW_CRYSTAL,10},
  {"Conquérant","Terminer 40 niveaux.",ST_WINS,0,40,RW_CRYSTAL,6},
  {"Invincible","Terminer 120 niveaux.",ST_WINS,0,120,RW_CRYSTAL,12},
  {"Grand bâtisseur","Atteindre 80 niveaux de bâtiments.",ST_BLD,0,80,RW_CRYSTAL,4},
  {"Cité de Fort-Rebond","Monter tous les bâtiments au niveau maximum.",ST_BLDMAX,0,BLD_COUNT,RW_CRYSTAL,10},
  {"Magnat","Rapporter 250 000 or au total.",ST_GOLD,0,250000,RW_CRYSTAL,4},
  {"Crésus","Rapporter 2 000 000 d'or au total.",ST_GOLD,0,2000000,RW_CRYSTAL,8},
  {"Sans fin IV","Atteindre l'étage 18 en mode infini.",ST_ENDLESS,0,18,RW_CRYSTAL,12},
  {"Combo divin","Atteindre un combo de 1 000.",ST_COMBO,0,1000,RW_CRYSTAL,4},
  {"Demi-dieu","Atteindre le niveau 50 de héros en une descente.",ST_PLEVEL,0,50,RW_CRYSTAL,4},
  {"Pèlerin","Vivre 100 événements dans le gouffre.",ST_EVENTS,0,100,RW_CRYSTAL,4},
  {"Quatre fusions","Réaliser 4 évolutions en une descente.",ST_FEAT,FEAT_4EVOS,1,RW_CRYSTAL,4},
  {"Mille d'un coup","Vaincre 1 000 monstres en une descente.",ST_FEAT,FEAT_KILLS_2000,1,RW_CRYSTAL,3},
  {"Pluie d'or","Rapporter 3 000 or en une seule descente.",ST_FEAT,FEAT_GOLD_3000,1,RW_CRYSTAL,3},
  {"Sang-froid abyssal","Vaincre un gardien des Profondeurs sans subir de dégâts.",ST_FEAT,FEAT_NOHIT_PROF,1,RW_CRYSTAL,5},
  {"Éclair des abysses","Vaincre les trois gardiens d'un niveau des Profondeurs en moins de 100 s au total.",ST_FEAT,FEAT_SPEED_PROF,1,RW_CRYSTAL,4},
  {"Minimaliste","Terminer le Cœur du Gouffre ou un niveau plus profond sans aucune évolution.",ST_FEAT,FEAT_NO_EVO_WIN,1,RW_CRYSTAL,3},
  {"Solitaire","Terminer le Cœur du Gouffre ou un niveau plus profond avec deux billes au plus.",ST_FEAT,FEAT_FEW_BALLS,1,RW_CRYSTAL,5},
  {"Le Cœur s'arrête","Vaincre le Cœur du Néant en moins de 60 secondes.",ST_FEAT,FEAT_FAST_VOID,1,RW_CRYSTAL,5},
  {"Dernier souffle","Terminer un niveau après avoir été ressuscité.",ST_FEAT,FEAT_REVIVE_WIN,1,RW_CRYSTAL,2},
  {"Collection complète","Débloquer toutes les billes et tous les objets.",ST_COLL,0,BASE_BALL_COUNT+PS_COUNT,RW_CRYSTAL,5},
  {"Explorateur des abysses","Terminer 25 descentes dans les Profondeurs.",ST_PROF_RUNS,0,25,RW_CRYSTAL,4},
  /* ---------------- v6 : niveaux un par un, duo de heros ---------------- */
  {"Duo de choc","Terminer un niveau avec deux héros.",ST_FEAT,FEAT_DUO,1,RW_CRYSTAL,2},
  {"Tour du Gouffre","Terminer les 12 niveaux.",ST_LVLS,0,BIOME_COUNT,RW_CRYSTAL,6},
  {"Carnet de route","Terminer un même niveau avec les 10 héros.",ST_LVLHEROES,0,CH_COUNT,RW_CRYSTAL,5},
  {"Grand carnet","Terminer chacun des 12 niveaux avec chacun des 10 héros.",ST_CHECKS,0,BIOME_COUNT*CH_COUNT,RW_CRYSTAL,15},
  {"Gardiens mineurs","Vaincre 50 gardiens mineurs.",ST_MBOSS,0,50,RW_CRYSTAL,3},
  /* ---------------- v7 : le Royaume oublié ---------------- */
  {"Le Jardin des Lucioles","Vaincre l'Arbre-Lune.",ST_BOSSN,BOSS_ARBRE,1,RW_BALL,BK_RACINE},
  {"Le Marais maudit","Vaincre le Roi-Crapaud.",ST_BOSSN,BOSS_CRAPAUD,1,RW_BALL,BK_ACIDE},
  {"Les Mines de cristal","Vaincre le Golem-Prisme.",ST_BOSSN,BOSS_PRISME,1,RW_BALL,BK_ECHO},
  {"Crépuscule","Vaincre le Soleil Noir.",ST_BOSSN,BOSS_SOLEIL,1,RW_CRYSTAL,8},
  {"Négoce","Rapporter 15 000 or au total.",ST_GOLD,0,15000,RW_HERO,CH_MARCHAND},
  {"Fortune royale","Rapporter 30 000 or au total.",ST_GOLD,0,30000,RW_BALL,BK_DOREE},
  {"Fleur fanée","Vaincre la Fleur dévoreuse.",ST_KILLKIND,EK_FLEUR,1,RW_PAS,PS_CAPE},
  {"Brumes dissipées","Vaincre la Sorcière des brumes.",ST_KILLKIND,EK_SORCIERE,1,RW_PAS,PS_TOTEM},
  {"Améthyste","Vaincre le Golem d'améthyste.",ST_KILLKIND,EK_GOLEM_CRISTAL,1,RW_PAS,PS_LOUPE},
  {"Jeter l'ancre","Terminer 10 descentes dans le Royaume oublié.",ST_ROY_RUNS,0,10,RW_PAS,PS_ANCRE},
  {"Le Grand Sylvain","Vaincre le Grand Sylvain.",ST_KILLKIND,EK_SYLVAIN,1,RW_GOLD,400},
  {"Sept têtes","Vaincre l'Hydre millénaire.",ST_KILLKIND,EK_HYDRE,1,RW_GOLD,500},
  {"Cœur de quartz","Vaincre le Gardien de quartz.",ST_KILLKIND,EK_QUARTZ,1,RW_CRYSTAL,2},
  {"L'énigme résolue","Vaincre le Sphinx d'or.",ST_KILLKIND,EK_SPHINX,1,RW_CRYSTAL,3},
  {"Royaume conquis","Terminer les 4 niveaux du Royaume oublié.",ST_ROYLVLS,0,4,RW_CRYSTAL,6},
  {"Pèlerin du Royaume","Terminer 5 niveaux du Royaume oublié.",ST_ROY_WINS,0,5,RW_CRYSTAL,5},
  {"Souverain du Royaume","Terminer 25 niveaux du Royaume oublié.",ST_ROY_WINS,0,25,RW_CRYSTAL,10},
  {"Explorateur du Royaume","Terminer 25 descentes dans le Royaume oublié.",ST_ROY_RUNS,0,25,RW_CRYSTAL,4},
  {"Soleil levant","Débloquer la profondeur 3 du Sanctuaire solaire.",ST_DEPTH3,0,3,RW_CRYSTAL,3},
  {"Zénith","Débloquer la profondeur 5 du Sanctuaire solaire.",ST_DEPTH3,0,5,RW_CRYSTAL,4},
  {"Couronne solaire","Débloquer la profondeur 8 du Sanctuaire solaire.",ST_DEPTH3,0,8,RW_CRYSTAL,6},
  {"Éclipse totale","Débloquer la profondeur 10 du Sanctuaire solaire.",ST_DEPTH3,0,10,RW_CRYSTAL,8},
  {"Soleil invaincu","Terminer le Sanctuaire solaire en profondeur 10.",ST_DWON,3,10,RW_CRYSTAL,20},
  {"Sang-froid royal","Vaincre un gardien du Royaume oublié sans subir de dégâts.",ST_FEAT,FEAT_NOHIT_ROY,1,RW_CRYSTAL,5},
  {"Éclair solaire","Vaincre les trois gardiens d'un niveau du Royaume oublié en moins de 100 s au total.",ST_FEAT,FEAT_SPEED_ROY,1,RW_CRYSTAL,4},
  {"Orage","Découvrir l'évolution Tonnerre.",ST_EVOK,BK_TONNERRE,1,RW_CRYSTAL,2},
  {"Toucher d'or","Découvrir l'évolution Midas.",ST_EVOK,BK_MIDAS,1,RW_CRYSTAL,2},
  {"Roncier","Découvrir l'évolution Ronces.",ST_EVOK,BK_RONCES,1,RW_CRYSTAL,2},
  {"Averse corrosive","Découvrir l'évolution Pluie acide.",ST_EVOK,BK_PLUIE,1,RW_CRYSTAL,2},
  {"Forêt éternelle","Amener la Druidesse au rang 10.",ST_RANK,CH_DRUIDE,10,RW_CRYSTAL,3},
  {"Fortune faite","Amener le Marchand au rang 10.",ST_RANK,CH_MARCHAND,10,RW_CRYSTAL,3},
  {"Au complet","Posséder les 10 héros.",ST_HEROES,0,CH_COUNT,RW_CRYSTAL,5},
  {"Compagnie complète","Amener les 10 héros au rang 5.",ST_RANKS,5,CH_COUNT,RW_CRYSTAL,6},
  {"Panthéon de légende","Amener les 10 héros au rang 10.",ST_RANKS,10,CH_COUNT,RW_CRYSTAL,30},
  {"Main verte","Monter la Serre au niveau maximum.",ST_BLDLV,BLD_SERRE,8,RW_CRYSTAL,3},
  {"Astronome","Monter l'Observatoire au niveau maximum.",ST_BLDLV,BLD_OBSERV,8,RW_CRYSTAL,3},
  {"Bestiaire du Royaume","Vaincre les 24 espèces du Royaume oublié.",ST_CODEX7,0,EK_COUNT-EK_FIRST_V7,RW_CRYSTAL,4},
  /* v8 : curiosites du decor */
  {"Curieux","Briser 25 curiosités (trésors ou pièges).",ST_CURIO,0,25,RW_GOLD,300},
  {"Pilleur du Gouffre","Briser 250 curiosités.",ST_CURIO,0,250,RW_CRYSTAL,4},
  {"Artificier","Vaincre 150 monstres avec les pièges du décor.",ST_CURIOK,0,150,RW_CRYSTAL,3},
  /* v9 : la grande mise a jour */
  {"Constellations","Monter l'Observatoire au niveau 3.",ST_BLDLV,BLD_OBSERV,3,RW_HERO,CH_ASTRONOME},
  {"Monte-en-l'air","Briser 40 curiosités.",ST_CURIO,0,40,RW_HERO,CH_VOLEUSE},
  {"Ombres portées","Vaincre 50 champions.",ST_CHAMPS,0,50,RW_BALL,BK_OMBRE},
  {"Rebonds en série","Atteindre un combo de 200.",ST_COMBO,0,200,RW_BALL,BK_RICOCHET},
  {"Cœur de magma","Vaincre 3 fois le Colosse de Magma.",ST_BOSSN,BOSS_COLOSSE,3,RW_BALL,BK_LAVE},
  {"Étoile du berger","Monter l'Observatoire au niveau 2.",ST_BLDLV,BLD_OBSERV,2,RW_BALL,BK_ETOILE},
  {"Gladiateur","Vaincre 10 gardiens.",ST_BOSSES,0,10,RW_MODE,MODE_RUSH},
  {"Lune noire","Découvrir l'évolution Éclipse.",ST_EVOK,BK_ECLIPSE,1,RW_CRYSTAL,2},
  {"Mauvais rêve","Découvrir l'évolution Cauchemar.",ST_EVOK,BK_CAUCHEMAR,1,RW_CRYSTAL,2},
  {"Billard","Découvrir l'évolution Carambole.",ST_EVOK,BK_CARAMBOLE,1,RW_CRYSTAL,2},
  {"Éruption","Découvrir l'évolution Volcan.",ST_EVOK,BK_VOLCAN,1,RW_CRYSTAL,2},
  {"Carte du ciel","Découvrir l'évolution Constellation.",ST_EVOK,BK_CONSTELLATION,1,RW_CRYSTAL,2},
  {"Naissance d'une étoile","Découvrir l'évolution Supernova.",ST_EVOK,BK_SUPERNOVA,1,RW_CRYSTAL,2},
  {"Le premier creuset","Construire le Creuset légendaire.",ST_BLDLV,BLD_CREUSET,1,RW_GOLD,500},
  {"Légende","Réaliser une fusion légendaire.",ST_LEG,0,1,RW_CRYSTAL,5},
  {"Origine du monde","Réaliser la fusion Big Bang.",ST_EVOK,BK_BIGBANG,1,RW_CRYSTAL,4},
  {"Dies irae","Réaliser la fusion Jugement dernier.",ST_EVOK,BK_JUGEMENT,1,RW_CRYSTAL,4},
  {"Silence de glace","Réaliser la fusion Zéro absolu.",ST_EVOK,BK_ZERO,1,RW_CRYSTAL,4},
  {"Le serpent éternel","Réaliser la fusion Ouroboros.",ST_EVOK,BK_OUROBOROS,1,RW_CRYSTAL,4},
  {"Panthéon des légendes","Réaliser toutes les fusions légendaires.",ST_LEGALL,0,LEG_COUNT,RW_CRYSTAL,10},
  {"Voie lactée","Amener l'Astronome au rang 10.",ST_RANK,CH_ASTRONOME,10,RW_CRYSTAL,3},
  {"Insaisissable","Amener la Voleuse d'ombres au rang 10.",ST_RANK,CH_VOLEUSE,10,RW_CRYSTAL,3},
  {"Maître fondeur","Monter le Creuset légendaire au niveau maximum.",ST_BLDLV,BLD_CREUSET,8,RW_CRYSTAL,4},
  {"Cabinet complet","Monter le Cabinet des curiosités au niveau maximum.",ST_BLDLV,BLD_CABINET,8,RW_CRYSTAL,3},
  {"Perfectionniste","Remplir 10 objectifs de niveau.",ST_OBJ,0,10,RW_CRYSTAL,3},
  {"Sans faute","Remplir les 36 objectifs de niveau.",ST_OBJ,0,3*BIOME_COUNT,RW_CRYSTAL,15},
  {"Rush","Terminer un Rush des gardiens.",ST_RUSH,0,1,RW_CRYSTAL,4},
  {"Chasseur de gardiens","Terminer le Rush des gardiens des trois destinations.",ST_RUSHDEST,0,DEST_COUNT,RW_CRYSTAL,8},
  {"Contre la montre","Terminer un Rush des gardiens en moins de 6 minutes.",ST_FEAT,FEAT_RUSH_FAST,1,RW_CRYSTAL,5},
  {"Mille et une merveilles","Briser 1 000 curiosités.",ST_CURIO,0,1000,RW_CRYSTAL,8},
  {"Dans l'ombre","Vaincre 500 monstres marqués.",ST_MARKK,0,500,RW_CRYSTAL,3},
  /* v10 : la mise a jour du gameplay */
  {"Retour à l'envoyeur","Terminer 5 niveaux.",ST_LVLS,0,5,RW_BALL,BK_BOOMERANG},
  {"Tremblement de terre","Vaincre 20 gardiens mineurs.",ST_MBOSS,0,20,RW_BALL,BK_SEISME},
  {"Maillon fort","Vaincre 100 champions.",ST_CHAMPS,0,100,RW_BALL,BK_CHAINE},
  {"Hors du temps","Vaincre 25 gardiens.",ST_BOSSES,0,25,RW_PAS,PS_PENDULE},
  {"Cap au sud","Briser 100 curiosités.",ST_CURIO,0,100,RW_PAS,PS_BOUSSOLE},
  {"Bibliothécaire","Découvrir 10 évolutions.",ST_EVO_DISC,0,10,RW_PAS,PS_GRIMOIRE},
  {"La traque","Terminer 30 descentes.",ST_RUNS,0,30,RW_HERO,CH_CHASSERESSE},
  {"Fondations","Vaincre 300 monstres avec des secousses (bille sismique, faille).",ST_QUAKEK,0,300,RW_HERO,CH_GEOMANCIEN},
  {"Lame du retour","Découvrir l'évolution Chakram.",ST_EVOK,BK_CHAKRAM,1,RW_CRYSTAL,2},
  {"Ligne de fracture","Découvrir l'évolution Faille.",ST_EVOK,BK_FAILLE,1,RW_CRYSTAL,2},
  {"Pieds et poings liés","Découvrir l'évolution Entrave.",ST_EVOK,BK_ENTRAVE,1,RW_CRYSTAL,2},
  {"Fin du monde","Réaliser la fusion Apocalypse.",ST_EVOK,BK_APOCALYPSE,1,RW_CRYSTAL,4},
  {"Apprenti sorcier","Terminer 3 niveaux.",ST_LVLS,0,3,RW_MODE,MUT_UNLOCK},
  {"Mutant","Gagner un niveau avec un mutateur.",ST_MUTMAX,0,1,RW_CRYSTAL,2},
  {"Savant fou","Gagner un niveau avec 4 mutateurs à la fois.",ST_MUTMAX,0,4,RW_CRYSTAL,4},
  {"Chaos","Gagner un niveau avec 8 mutateurs à la fois.",ST_MUTMAX,0,8,RW_CRYSTAL,8},
  {"Tout ou rien","Gagner un niveau avec les 12 mutateurs.",ST_MUTMAX,0,MOD_COUNT,RW_CRYSTAL,15},
  {"Habitué des mutations","Gagner 25 niveaux avec des mutateurs.",ST_MUTWIN,0,25,RW_CRYSTAL,5},
  {"Démineur","Vaincre 30 champions explosifs.",ST_CHAMPK,CHAMP_EXPLOSIF,30,RW_CRYSTAL,2},
  {"Couper la tête","Vaincre 30 champions invocateurs.",ST_CHAMPK,CHAMP_INVOCATEUR,30,RW_CRYSTAL,2},
  {"Pas de quartier","Vaincre 30 champions soigneurs.",ST_CHAMPK,CHAMP_SOIGNEUR,30,RW_CRYSTAL,2},
  {"Chasseur de spectres","Vaincre 30 champions spectraux.",ST_CHAMPK,CHAMP_SPECTRAL,30,RW_CRYSTAL,2},
  {"Galerie des champions","Vaincre un champion de chaque pouvoir (8).",ST_CHAMPALL,0,CHAMP_COUNT-1,RW_CRYSTAL,4},
  {"Chaîne de commandement","Vaincre 300 monstres par les chaînes.",ST_LINKK,0,300,RW_CRYSTAL,3},
  {"Revenante","Amener la Chasseresse au rang 10.",ST_RANK,CH_CHASSERESSE,10,RW_CRYSTAL,3},
  {"Gardien de la roche","Amener le Géomancien au rang 10.",ST_RANK,CH_GEOMANCIEN,10,RW_CRYSTAL,3},
};
const int ACH_COUNT = (int)(sizeof ACHS/sizeof ACHS[0]);
_Static_assert(sizeof ACHS/sizeof ACHS[0] <= ACH_MAX, "ACH_MAX trop petit");

static int popcount32(uint32_t v){ int n=0; while(v){ n+=v&1; v>>=1; } return n; }

/* =====================================================================
   progression longue : rangs, maitrise, bestiaire, profondeurs, 100 %
   ===================================================================== */
static int rank_cum(int r){ int t=0; for(int k=0;k<r-1;k++) t+=3000+2500*k+600*k*k; return t; }   /* rang 2 : 3 000 XP ... rang 10 : 239 400 XP */   /* xp pour atteindre le rang r */
int hero_rank(int ch){ int x=S.hero_xp[ch], r=1; while(r<RANK_MAX && x>=rank_cum(r+1)) r++; return r; }
int hero_rank_xp(int ch,int *need){ int r=hero_rank(ch); if(r>=RANK_MAX){ *need=0; return 0; } *need=rank_cum(r+1)-rank_cum(r); return S.hero_xp[ch]-rank_cum(r); }
float hero_dmg_bonus(int ch){ return 0.02f*(hero_rank(ch)-1); }
int hero_hp_bonus(int ch){ return 3*(hero_rank(ch)-1); }
bool ball_masterable(int k){ return k>=BK_FIRST_BASE && k<=BK_LAST_EVO; }
int ball_star_need(int k,int star){ static const int B[3]={200,1500,5000}, E[3]={100,750,2500}; star=clampi(star,1,3); return IS_EVO(k)?E[star-1]:B[star-1]; }
int ball_stars(int k){ if(!ball_masterable(k)) return 0; int s=0; for(int st=1;st<=3;st++) if(S.ball_kills[k]>=ball_star_need(k,st)) s=st; return s; }
int total_stars(void){ int n=0; for(int k=BK_FIRST_BASE;k<=BK_LAST_EVO;k++) n+=ball_stars(k); return n; }
int bestiary_need(int ek,int tier){ bool el=(ENEMIES[ek].flags&EF_ELITE)!=0; if(tier<=1) return 1; if(tier==2) return el?25:150; return el?200:2500; }
int bestiary_tier(int ek){ if(ek<0||ek>=EK_COUNT) return 0; int v=S.kills_en[ek], t=0; for(int k=1;k<=3;k++) if(v>=bestiary_need(ek,k)) t=k; return t; }
int boss_tier(int b){ int v=S.boss_kills[b]; return v>=20?3:(v>=5?2:(v>=1?1:0)); }
/* les Profondeurs et le Royaume oublie s'ouvrent ensemble, une fois le Coeur du Gouffre vaincu */
bool dest_unlocked(int d){ return d==0 || S.boss_kills[BOSS_OEIL]>0 || lvl_unlocked(d*LEVELS_PER_DEST); }
/* ---------------- les douze niveaux ---------------- */
int lvl_maxwon(int L){ if(L<0||L>=BIOME_COUNT) return 0; for(int d=DEPTH_MAX;d>=1;d--) if(S.lvl_done[L][d-1]) return d; return 0; }
/* niveau a terminer pour ouvrir L : le precedent, sauf le premier niveau d'une destination (le Coeur du Gouffre) */
int lvl_prereq(int L){ if(L<=0) return -1; if(L%LEVELS_PER_DEST==0) return LEVELS_PER_DEST-1; return L-1; }
bool lvl_unlocked(int L){ if(L<=0) return true; if(L>=BIOME_COUNT) return false; return lvl_maxwon(lvl_prereq(L))>0; }
int lvl_depth_max(int L){ return clampi(lvl_maxwon(L)+1,1,DEPTH_MAX); }
unsigned lvl_heroes(int L,int depth){
  if(L<0||L>=BIOME_COUNT) return 0;
  if(depth>=1 && depth<=DEPTH_MAX) return S.lvl_done[L][depth-1];
  unsigned m=0; for(int d=0;d<DEPTH_MAX;d++) m|=S.lvl_done[L][d]; return m;
}
unsigned lvl_heroes_diff(int L,int depth,int diff){
  if(L<0||L>=BIOME_COUNT) return 0;
  if(diff<0||diff>=DIFF_COUNT) return lvl_heroes(L,depth);
  if(depth>=1 && depth<=DEPTH_MAX) return S.lvl_done_d[diff][L][depth-1];
  unsigned m=0; for(int d=0;d<DEPTH_MAX;d++) m|=S.lvl_done_d[diff][L][d]; return m;
}
int lvl_count_unlocked(void){ int n=0; for(int L=0;L<BIOME_COUNT;L++) if(lvl_unlocked(L)) n++; return n; }
int lvl_checks(void){ int n=0; for(int L=0;L<BIOME_COUNT;L++) n+=popcount32(lvl_heroes(L,0)); return n; }
void lvl_derive(void){
  /* profondeurs d'une destination : celles de son dernier niveau (l'Oeil, le Coeur du Neant) */
  for(int d=0;d<DEST_COUNT;d++){ int m=lvl_maxwon((d+1)*LEVELS_PER_DEST-1); S.depth_won[d]=maxi(S.depth_won[d],m); }
  S.depth_max=maxi(S.depth_max,clampi(S.depth_won[0]+1,1,DEPTH_MAX));
  S.depth_max2=maxi(S.depth_max2,clampi(S.depth_won[1]+1,1,DEPTH_MAX));
  S.depth_max3=maxi(S.depth_max3,clampi(S.depth_won[2]+1,1,DEPTH_MAX));
  S.best_level=maxi(S.best_level,lvl_count_unlocked());
}
/* deux gardiens mineurs par niveau : une grande creature du biome, puis son elite */
static const int MB_KIND[BIOME_COUNT][2]={{EK_CRANE,EK_OSSUAIRE},{EK_MANTE,EK_SPORANGE},{EK_ARMURE,EK_GOLEM},{EK_CHEVALIER,EK_HORREUR},
  {EK_HARFANG,EK_YETI},{EK_SIRENE,EK_KRAKEN},{EK_GARDIENNE,EK_BOURDON},{EK_HERAUT,EK_COLOSSE_NEANT},
  {EK_FLEUR,EK_SYLVAIN},{EK_SORCIERE,EK_HYDRE},{EK_GOLEM_CRISTAL,EK_QUARTZ},{EK_PRETRE,EK_SPHINX}};
static const char *MB_NAME[BIOME_COUNT][2]={{"Le Crâne ancestral","L'Ossuaire vorace"},{"La Mante reine","Le Grand Sporange"},
  {"L'Armure maudite","Le Golem de fonte"},{"Le Chevalier déchu","L'Horreur rampante"},{"Le Grand Harfang","Le Yéti des cimes"},
  {"La Sirène noire","Le Kraken"},{"La Gardienne d'ambre","Le Bourdon royal"},{"Le Héraut du vide","Le Colosse du Néant"},
  {"La Fleur dévoreuse","Le Grand Sylvain"},{"La Sorcière des brumes","L'Hydre millénaire"},{"Le Golem d'améthyste","Le Gardien de quartz"},{"Le Grand Prêtre","Le Sphinx d'or"}};
static const char *LVL_SHORT[BIOME_COUNT]={"LES CATACOMBES","LA FORÊT FONGIQUE","LES FORGES ENGLOUTIES","LE CŒUR DU GOUFFRE",
  "LES GROTTES DE GIVRE","LA CITÉ ENGLOUTIE","LA RUCHE D'AMBRE","LE NÉANT",
  "LE JARDIN DES LUCIOLES","LE MARAIS MAUDIT","LES MINES DE CRISTAL","LE SANCTUAIRE SOLAIRE"};
int lvl_mb_kind(int L,int i){ return MB_KIND[clampi(L,0,BIOME_COUNT-1)][clampi(i,0,1)]; }
const char *lvl_mb_name(int L,int i){ return MB_NAME[clampi(L,0,BIOME_COUNT-1)][clampi(i,0,1)]; }
const char *lvl_short(int L){ return LVL_SHORT[clampi(L,0,BIOME_COUNT-1)]; }
int depth_max_of(int d){ return d==2?S.depth_max3:(d?S.depth_max2:S.depth_max); }
int *depth_sel_of(int d){ return d==2?&S.depth_sel3:(d?&S.depth_sel2:&S.depth_sel); }
static int coll_count(void){ int n=0; for(int k=BK_FIRST_BASE;k<=BK_LAST_BASE;k++) n+=ball_unlocked(k); for(int k=0;k<PS_COUNT;k++) n+=pas_unlocked(k); return n; }
static const char *COMP_NAME[COMP_CATS]={"Défis","Niveaux et profondeurs","Rangs des héros","Maîtrise des billes","Bestiaire","Campement","Collection","Mode infini"};
static const float COMP_W[COMP_CATS]={25,20,15,10,10,10,5,5};
float completion_part(int cat,float *weight,const char **name){
  if(weight) *weight=COMP_W[cat]; if(name) *name=COMP_NAME[cat];
  switch(cat){
    case 0: { int n=0; for(int i=0;i<ACH_COUNT;i++) n+=S.ach[i]; return (float)n/ACH_COUNT; }
    case 1: { int n=0; for(int L=0;L<BIOME_COUNT;L++) n+=lvl_maxwon(L); return 0.7f*n/(float)(BIOME_COUNT*DEPTH_MAX)+0.3f*lvl_checks()/(float)(BIOME_COUNT*CH_COUNT); }
    case 2: { int n=0; for(int c=0;c<CH_COUNT;c++) n+=hero_rank(c)-1; return n/(float)(CH_COUNT*(RANK_MAX-1)); }
    case 3: return total_stars()/(float)STARS_MAX;
    case 4: { int n=0; for(int k=0;k<EK_COUNT;k++) n+=bestiary_tier(k); for(int b=0;b<BOSS_COUNT;b++) n+=boss_tier(b); return n/(3.0f*(EK_COUNT+BOSS_COUNT)); }
    case 5: { int n=0,m=0; for(int b=0;b<BLD_COUNT;b++){ n+=S.bld[b]; m+=BUILDINGS[b].max; } return (float)n/m; }
    case 6: { int n=coll_count()+popcount32(S.evo_seen&((1u<<EVO_COUNT)-1)); for(int c=0;c<CH_COUNT;c++) n+=S.unlocked[c]; return n/(float)(BASE_BALL_COUNT+PS_COUNT+EVO_COUNT+CH_COUNT); }
    default: return mini(S.endless_best,18)/18.0f;
  }
}
float completion_pct(void){ float t=0; for(int c=0;c<COMP_CATS;c++){ float w; float f=completion_part(c,&w,NULL); t+=clampf(f,0,1)*w; } return t; }

int ach_progress(int i,int *target){
  const AchDef *a=&ACHS[i]; *target=a->target; int v=0;
  switch(a->stat){
    case ST_KILLS: v=S.total_kills; break;
    case ST_RUNS: v=S.runs; break;
    case ST_WINS: v=S.wins; break;
    case ST_LEVEL: v=S.best_level; break;
    case ST_BOSSES: v=S.bosses_total; break;
    case ST_BOSS0: case ST_BOSS1: case ST_BOSS2: case ST_BOSS3: v=S.boss_kills[a->stat-ST_BOSS0]; break;
    case ST_ELITES: v=S.elites; break;
    case ST_EVOS: v=S.evolutions; break;
    case ST_EVO_DISC: v=popcount32(S.evo_seen); break;
    case ST_COMBO: v=S.best_combo; break;
    case ST_PLEVEL: v=S.best_plevel; break;
    case ST_GOLD: v=S.gold_total; break;
    case ST_DEPTH: v=S.depth_max; break;
    case ST_HERO_WINS: v=popcount32(S.hero_wins); break;
    case ST_HEROES: for(int c=0;c<CH_COUNT;c++) v+=S.unlocked[c]; break;
    case ST_BLD: for(int b=0;b<BLD_COUNT;b++) v+=S.bld[b]; break;
    case ST_EVENTS: v=S.events_done; break;
    case ST_ENDLESS: v=S.endless_best; break;
    case ST_DAILY: v=S.daily_done; break;
    case ST_DAILY_WIN: v=S.daily_wins; break;
    case ST_CODEX: for(int k=0;k<EK_COUNT;k++) v+=S.kills_en[k]>0; break;
    case ST_FEAT: v=(S.feats>>a->param)&1; break;
    case ST_BOSSN: v=S.boss_kills[clampi(a->param,0,BOSS_COUNT-1)]; break;
    case ST_DEPTH2: v=S.depth_max2; break;
    case ST_DWON: v= a->param==2 ? maxi(S.depth_won[0],maxi(S.depth_won[1],S.depth_won[2])) : S.depth_won[clampi(a->param==3?2:a->param,0,DEST_COUNT-1)]; break;
    case ST_PROF_WINS: v=S.prof_wins; break;
    case ST_PROF_RUNS: v=S.prof_runs; break;
    case ST_RANKS: for(int c=0;c<CH_COUNT;c++) v+=hero_rank(c)>=a->param; break;
    case ST_RANK: v=hero_rank(clampi(a->param,0,CH_COUNT-1)); break;
    case ST_STARS: v=total_stars(); break;
    case ST_STAR3: for(int k=BK_FIRST_BASE;k<=BK_LAST_EVO;k++) v+=ball_stars(k)>=3; break;
    case ST_BEST: for(int k=0;k<EK_COUNT;k++) v+=bestiary_tier(k)>=a->param; break;
    case ST_BLDMAX: for(int b=0;b<BLD_COUNT;b++) v+=S.bld[b]>=BUILDINGS[b].max; break;
    case ST_CHAMPS: v=S.champions; break;
    case ST_BOSSDISC: for(int b=0;b<BOSS_COUNT;b++) v+=S.boss_kills[b]>0; break;
    case ST_BOSSMIN: v=1<<30; for(int b=0;b<BOSS_COUNT;b++) v=mini(v,S.boss_kills[b]); break;
    case ST_COLL: v=coll_count(); break;
    case ST_LVLS: for(int L=0;L<BIOME_COUNT;L++) v+=lvl_maxwon(L)>0; break;
    case ST_LVLHEROES: for(int L=0;L<BIOME_COUNT;L++) v=maxi(v,popcount32(lvl_heroes(L,0))); break;
    case ST_CHECKS: v=lvl_checks(); break;
    case ST_MBOSS: v=S.mb_kills; break;
    case ST_DEPTH3: v=S.depth_max3; break;
    case ST_ROY_WINS: v=S.roy_wins; break;
    case ST_CURIO: v=S.curios; break;
    case ST_CURIOK: v=S.curio_kills; break;
    case ST_LEG: v=S.legendaries; break;
    case ST_LEGALL: for(int k=BK_FIRST_LEG;k<=BK_LAST_LEG;k++) v+=(S.evo_seen>>(k-BK_FIRST_EVO))&1; break;
    case ST_OBJ: v=obj_total(); break;
    case ST_RUSH: v=S.rush_wins; break;
    case ST_RUSHDEST: v=popcount32((uint32_t)S.rush_dest); break;
    case ST_MARKK: v=S.mark_kills; break;
    case ST_MUTWIN: v=S.mut_wins; break;
    case ST_MUTMAX: v=S.mut_best; break;
    case ST_CHAMPK: v=S.champ_kinds[clampi(a->param,1,CHAMP_COUNT-1)]; break;
    case ST_CHAMPALL: for(int c=1;c<CHAMP_COUNT;c++) v+=S.champ_kinds[c]>0; break;
    case ST_QUAKEK: v=S.quake_kills; break;
    case ST_LINKK: v=S.link_kills; break;
    case ST_ROY_RUNS: v=S.roy_runs; break;
    case ST_KILLKIND: v=S.kills_en[clampi(a->param,0,EK_COUNT-1)]; break;
    case ST_EVOK: v=IS_EVO(a->param) ? (int)((S.evo_seen>>(a->param-BK_FIRST_EVO))&1) : 0; break;
    case ST_ROYLVLS: for(int L=2*LEVELS_PER_DEST;L<3*LEVELS_PER_DEST && L<BIOME_COUNT;L++) v+=lvl_maxwon(L)>0; break;
    case ST_BLDLV: v=S.bld[clampi(a->param,0,BLD_COUNT-1)]; break;
    case ST_CODEX7: for(int k=EK_FIRST_V7;k<EK_COUNT;k++) v+=S.kills_en[k]>0; break;
  }
  if(v>*target) v=*target;
  return v;
}

void reward_text(int i,char *b,int n){
  const AchDef *a=&ACHS[i];
  switch(a->rw){
    case RW_GOLD: snprintf(b,n,"^1+%d "IC_COIN,a->rw_val); break;
    case RW_CRYSTAL: snprintf(b,n,"^6+%d "IC_GEM,a->rw_val); break;
    case RW_BALL: snprintf(b,n,"^4Bille : %s",BALLS[a->rw_val].name); break;
    case RW_PAS: snprintf(b,n,"^2Objet : %s",PASSIVES[a->rw_val].name); break;
    case RW_HERO: snprintf(b,n,"^5Héros : %s",CHARS[a->rw_val].name); break;
    default: if(a->rw_val==MUT_UNLOCK) snprintf(b,n,"^6Mutateurs"); else snprintf(b,n,"^6Mode : %s",a->rw_val==MODE_ENDLESS?"Infini":(a->rw_val==MODE_RUSH?"Rush des gardiens":"Défi du jour")); break;
  }
}

/* ---------------- deblocages ---------------- */
#define BASE_BALLS ((1ull<<BK_FER)|(1ull<<BK_FEU)|(1ull<<BK_GLACE)|(1ull<<BK_FOUDRE)|(1ull<<BK_VENIN)|(1ull<<BK_EPINE)|(1ull<<BK_BOMBE)|(1ull<<BK_SPECTRE))
#define BASE_PAS ((1u<<PS_FORCE)|(1u<<PS_CADENCE)|(1u<<PS_BOTTES)|(1u<<PS_COEUR)|(1u<<PS_RESSORT)|(1u<<PS_SAC)|(1u<<PS_LYNX)|(1u<<PS_LANTERNE))
bool ball_unlocked(int k){ return ((BASE_BALLS|S.balls_unlocked)>>k)&1; }
bool pas_unlocked(int k){ return ((BASE_PAS|S.pas_unlocked)>>k)&1; }
int ach_for_reward(int rw,int val){ for(int i=0;i<ACH_COUNT;i++) if(ACHS[i].rw==rw && ACHS[i].rw_val==val) return i; return -1; }
bool hero_available(int ch){ return S.unlocked[ch]; }
bool mode_unlocked(int m){ if(m==MODE_NORMAL) return true; int i=ach_for_reward(RW_MODE,m); return i>=0 && S.ach[i]; }

/* ---------------- notifications ---------------- */
static int tq[16]; static int tq_n=0; static float tq_t=0;
static int run_list[16]; static int run_n=0;
void meta_run_begin(void){ run_n=0; }
int meta_run_achs(int *out,int max){ int n=mini(run_n,max); for(int i=0;i<n;i++) out[i]=run_list[i]; return n; }

static void grant(int i){
  const AchDef *a=&ACHS[i];
  switch(a->rw){
    case RW_GOLD: S.gold+=a->rw_val; break;
    case RW_CRYSTAL: S.crystals+=a->rw_val; S.crystals_total+=a->rw_val; break;
    case RW_BALL: S.balls_unlocked|=1ull<<a->rw_val; break;
    case RW_PAS: S.pas_unlocked|=1u<<a->rw_val; break;
    case RW_HERO: S.unlocked[a->rw_val]=true; break;
    default: break;
  }
}
void meta_check(void){
  bool any=false;
  for(int pass=0;pass<2;pass++)   /* un deblocage peut en accomplir un autre */
  for(int i=0;i<ACH_COUNT && i<ACH_MAX;i++){
    if(S.ach[i]) continue;
    int t; int v=ach_progress(i,&t);
    if(v>=t){
      S.ach[i]=true; grant(i); any=true;
      if(tq_n<16) tq[tq_n++]=i;
      if(run_n<16) run_list[run_n++]=i;
      if(g_state==GS_RUN) R.new_achs++;
    }
  }
  if(any) save_write();
}
void meta_feat(int f){ if(!((S.feats>>f)&1)){ S.feats|=1u<<f; meta_check(); } }
void meta_note_kill(int kind){ if(kind>=0 && kind<EK_COUNT) S.kills_en[kind]++; S.total_kills++; }
int meta_new_count(void){ int n=0; for(int i=0;i<ACH_COUNT;i++) n+=S.ach[i]; return n; }

void draw_meta_toast(float dt){
  { static int nt=-1; if(nt<0) nt=getenv("GOUFFRE_NOTOAST")?1:0; if(nt){ tq_n=0; return; } }   /* captures d'ecran */
  if(tq_n<=0) return;
  if(tq_t==0) sfx(SFX_ACHIEVE);
  tq_t+=dt;
  float T=tq_n>3?2.0f:3.4f; float a=tq_t<0.3f ? tq_t/0.3f : (tq_t>T-0.3f ? (T-tq_t)/0.3f : 1);
  a=clampf(a,0,1);
  int i=tq[0]; char rb[96]; reward_text(i,rb,sizeof rb);
  if(g_state==GS_RUN){
    int w=156, h=46, x=SCREEN_W-w-2+(int)((1-a)*(w+4)), y=SCREEN_H-h-50;
    gfx_rect_a(x,y,w,h,HEX(0x0e0a14),240); gfx_frame(x,y,w,h,HEX(0xc9a24a)); gfx_frame(x+2,y+2,w-4,h-4,HEX(0x3a2a1a));
    gfx_glow((float)x+16,(float)y+14,20,HEX(0xffc040),0.35f);
    text_draw(x+16,y+5,IC_STAR,HEX(0xffd84a),2,AL_CENTER,TX_OUTLINE);
    text_draw(x+30,y+5,"DÉFI ACCOMPLI",HEX(0xc9a24a),1,AL_LEFT,0);
    text_draw(x+8,y+19,ACHS[i].name,HEX(0xffffff),1,AL_LEFT,TX_SHADOW);
    text_draw(x+8,y+31,rb,HEX(0xf0e6d0),1,AL_LEFT,TX_SHADOW);
  } else {
    int w=300, h=40, x=SCREEN_W/2-w/2, y=(int)(-h+(h+36)*a);
    gfx_rect_a(x,y,w,h,HEX(0x0e0a14),235);
    gfx_frame(x,y,w,h,HEX(0xc9a24a)); gfx_frame(x+2,y+2,w-4,h-4,HEX(0x3a2a1a));
    gfx_glow((float)x+22,(float)y+20,26,HEX(0xffc040),0.35f);
    text_draw(x+22,y+8,IC_STAR,HEX(0xffd84a),2,AL_CENTER,TX_OUTLINE);
    text_draw(x+44,y+6,"DÉFI ACCOMPLI",HEX(0xc9a24a),1,AL_LEFT,0);
    text_draw(x+44,y+18,ACHS[i].name,HEX(0xffffff),1,AL_LEFT,TX_SHADOW);
    text_draw(x+w-10,y+18,rb,HEX(0xf0e6d0),1,AL_RIGHT,TX_SHADOW);
  }
  if(tq_t>=T){ for(int k=1;k<tq_n;k++) tq[k-1]=tq[k]; tq_n--; tq_t=0; }
}

/* ---------------- defi du jour ---------------- */
int daily_seed_today(void){
  time_t now=time(NULL); struct tm *lt=localtime(&now); if(!lt) return 20260101;
  return (lt->tm_year+1900)*10000+(lt->tm_mon+1)*100+lt->tm_mday;
}
int daily_level_today(void){
  uint32_t h=(uint32_t)daily_seed_today()*2246822519u; h^=h>>13; h*=3266489917u; h^=h>>16;
  int k=(int)(h%(uint32_t)maxi(1,lvl_count_unlocked()));   /* parmi les niveaux deja ouverts */
  for(int L=0;L<BIOME_COUNT;L++) if(lvl_unlocked(L)){ if(k==0) return L; k--; }
  return 0;
}
void daily_setup(int seed,int *hero,int *mods2){
  uint32_t h=(uint32_t)seed*2654435761u; h^=h>>15; h*=2246822519u; h^=h>>13;
  *hero=(int)(h%CH_COUNT);
  int a=(int)((h>>8)%MOD_DAILY_N), b=(int)((h>>16)%(MOD_DAILY_N-1)); if(b>=a) b++;
  mods2[0]=a; mods2[1]=b;
}

/* =====================================================================
   ecrans : Defis et Codex
   ===================================================================== */
static int meta_scr=META_ACH, meta_ret=GS_TITLE; static int ach_sel=0, ach_scroll=0;
static int cx_tab=0, cx_sel=0, cx_scroll=0;
void meta_debug_tab(int t){ cx_tab=clampi(t,0,3); }
extern float g_boss_draw_scale;
static void star_pips(int x,int y,int n,int max,uint32_t on){ for(int i=0;i<max;i++){ gfx_rect(x+i*6,y,4,2,i<n?on:HEX(0x3a3242)); } }
void meta_open(int screen,int ret){ meta_scr=screen; meta_ret=ret; ach_sel=0; ach_scroll=0; cx_tab=0; cx_sel=0; cx_scroll=0; goto_state(GS_META); }

extern void menu_bg_public(float dt,int amb);
static void meta_header(const char *title,const char *sub){
  text_draw(SCREEN_W/2,8,title,HEX(0xffd84a),2,AL_CENTER,TX_OUTLINE);
  if(sub) text_draw(SCREEN_W/2,32,sub,HEX(0xb0a8c0),1,AL_CENTER,TX_SHADOW);
}
static void ach_frame(void){
  int done=meta_new_count();
  char b[160]; snprintf(b,sizeof b,"%d / %d accomplis",done,ACH_COUNT);
  meta_header("DÉFIS",b);
  int px=40, py=48, pw=560, ph=276, rh=27; int vis=(ph-8)/rh;
  ui_fancy_panel(px,py,pw,ph);
  if(A.down){ ach_sel=mini(ach_sel+1,ACH_COUNT-1); sfx(SFX_HOVER); }
  if(A.up){ ach_sel=maxi(ach_sel-1,0); sfx(SFX_HOVER); }
  if(A.right){ ach_sel=mini(ach_sel+vis,ACH_COUNT-1); sfx(SFX_HOVER); }
  if(A.left){ ach_sel=maxi(ach_sel-vis,0); sfx(SFX_HOVER); }
  if(A.wheel){ ach_scroll-=A.wheel*3; }
  if(A.up||A.down||A.left||A.right){ if(ach_sel<ach_scroll) ach_scroll=ach_sel; if(ach_sel>=ach_scroll+vis) ach_scroll=ach_sel-vis+1; }
  ach_scroll=clampi(ach_scroll,0,maxi(0,ACH_COUNT-vis));
  gfx_clip(px+2,py+3,px+pw-2,py+ph-3);
  for(int r=0;r<vis+1;r++){
    int i=ach_scroll+r; if(i>=ACH_COUNT) break;
    int y=py+5+r*rh, x=px+8, w=pw-16;
    bool sel=i==ach_sel; bool ok=S.ach[i];
    if(mouse_in(x,y,w,rh-2) && A.mouse_moved) ach_sel=i;
    gfx_rect(x,y,w,rh-3,sel?HEX(0x2a2038):(ok?HEX(0x1c1a14):HEX(0x16121c)));
    if(sel) gfx_frame(x,y,w,rh-3,HEX(0xd8b060));
    text_draw(x+12,y+3,IC_STAR,ok?HEX(0xffd84a):HEX(0x3a3242),2,AL_CENTER,0);
    text_draw(x+26,y+2,ACHS[i].name,ok?HEX(0xffd84a):HEX(0xf0e6d0),1,AL_LEFT,TX_SHADOW);
    text_draw(x+26,y+13,ACHS[i].desc,ok?HEX(0x8a8270):HEX(0x9a94a8),1,AL_LEFT,0);
    char rb[96]; reward_text(i,rb,sizeof rb); text_draw(x+w-8,y+2,rb,HEX(0xf0e6d0),1,AL_RIGHT,0);
    if(ok) text_draw(x+w-8,y+13,"^2Accompli",HEX(0x8be05a),1,AL_RIGHT,0);
    else { int tg; int v=ach_progress(i,&tg); if(tg>1){ ui_bar(x+w-108,y+15,70,4,(float)v/tg,HEX(0x5ab8ff),HEX(0x1e1828)); snprintf(b,sizeof b,"%d/%d",v,tg); text_draw(x+w-8,y+12,b,HEX(0x8a82a0),1,AL_RIGHT,0); } }
  }
  gfx_noclip();
  /* ascenseur */
  int sh=ph-12, th=maxi(12,sh*vis/ACH_COUNT); int ty=py+6+(sh-th)*ach_scroll/maxi(1,ACH_COUNT-vis);
  gfx_rect(px+pw-6,py+6,3,sh,HEX(0x1e1828)); gfx_rect(px+pw-6,ty,3,th,HEX(0x8a7a5a));
}

static const char *TABS[4]={"Monstres","Gardiens","Billes","Objets"};
static int tab_count(int t){ return t==0?EK_COUNT:(t==1?BOSS_COUNT:(t==2?(BK_LAST_LEG-BK_FIRST_BASE+1):PS_COUNT)); }
static bool cx_known(int t,int i){
  switch(t){
    case 0: return S.kills_en[i]>0;
    case 1: return S.boss_kills[i]>0;
    case 2: { int k=BK_FIRST_BASE+i; return IS_EVO(k) ? ((S.evo_seen>>(k-BK_FIRST_EVO))&1) : ball_unlocked(k); }
    default: return pas_unlocked(i);
  }
}
static void cx_cell(int t,int i,int x,int y,bool known){
  if(t==0){
    const EnemyDef *d=&ENEMIES[i];
    if(d->sprite<0){ art_draw_tentacle((float)x+15,(float)y+26,g_time,18,known?HEX(0xa35fe0):HEX(0x241e2c),known?HEX(0x4a2266):HEX(0x18141f),-PI*0.5f); return; }
    DrawOpt o=dopt(); const Sprite *s=&SPR[d->sprite]; float sc=1.0f; if(SPR_LO[d->sprite].w>18){ s=&SPR_LO[d->sprite]; } o.sx=o.sy=sc; if(!known){ o.flash=HEX(0x241e2c); o.flash_a=255; }
    draw_spr_c(s,(float)x+15,(float)y+14,&o);
    if(known) star_pips(x+6,y+27,bestiary_tier(i),3,HEX(0x8be05a));
  } else if(t==1){
    gfx_clip(x+1,y+1,x+67,y+89);
    g_boss_draw_scale=0.55f; art_draw_boss(i,(float)x+34,(float)y+43,g_time,0,0,0,0,0); g_boss_draw_scale=1.0f;
    if(!known) gfx_rect_a(x,y,68,90,HEX(0x0e0a14),235);
    gfx_noclip();
    if(known) star_pips(x+17,y+82,boss_tier(i),3,HEX(0xffd84a));
  } else if(t==2){
    int k=BK_FIRST_BASE+i; DrawOpt o=dopt(); if(!known){ o.flash=HEX(0x241e2c); o.flash_a=255; }
    icon_draw(k,(float)x+15,(float)y+13,25,&o);
    if(known) star_pips(x+6,y+26,ball_stars(k),3,HEX(0xffd84a));
  } else {
    DrawOpt o=dopt(); if(!known){ o.flash=HEX(0x241e2c); o.flash_a=255; }
    icon_draw(ICON_PAS(i),(float)x+15,(float)y+14,27,&o);
  }
}
static void codex_frame(void){
  int known_total=0, n=tab_count(cx_tab); for(int i=0;i<n;i++) known_total+=cx_known(cx_tab,i);
  meta_header("CODEX",NULL);
  /* onglets */
  for(int t=0;t<4;t++){
    int w=110, x=SCREEN_W/2-2*w-12+t*(w+8), y=30;
    bool on=t==cx_tab; int nk=0, nt=tab_count(t); for(int i=0;i<nt;i++) nk+=cx_known(t,i);
    ui_panel(x,y,w,18,on?HEX(0x3a2a4e):HEX(0x1e1828),on?HEX(0xd8b060):HEX(0x5a4a6a));
    char b[48]; snprintf(b,sizeof b,"%s %d/%d",TABS[t],nk,nt); text_draw(x+w/2,y+4,b,on?HEX(0xffffff):HEX(0xa8a0b8),1,AL_CENTER,0);
    if(mouse_in(x,y,w,18) && A.click && cx_tab!=t){ cx_tab=t; cx_sel=0; cx_scroll=0; sfx(SFX_HOVER); }
  }
  if(A.tab_l){ cx_tab=(cx_tab+3)%4; cx_sel=0; cx_scroll=0; sfx(SFX_HOVER); }
  if(A.tab_r){ cx_tab=(cx_tab+1)%4; cx_sel=0; cx_scroll=0; sfx(SFX_HOVER); }
  n=tab_count(cx_tab); if(cx_sel>=n) cx_sel=n-1;
  int gx=24, gy=56; int cols = cx_tab==1 ? 4 : 8; int cw = cx_tab==1 ? 72 : 34, ch = cx_tab==1 ? 94 : 34;
  int rows=(n+cols-1)/cols, vis=cx_tab==1 ? 3 : 8;   /* v7 : la grille defile (79 monstres, 12 gardiens) */
  ui_fancy_panel(gx-8,gy-6,cols*cw+12,288);
  bool nav=false;
  if(A.left){ cx_sel=(cx_sel+n-1)%n; sfx(SFX_HOVER); nav=true; } if(A.right){ cx_sel=(cx_sel+1)%n; sfx(SFX_HOVER); nav=true; }
  if(A.up){ cx_sel=maxi(0,cx_sel-cols); sfx(SFX_HOVER); nav=true; } if(A.down){ cx_sel=mini(n-1,cx_sel+cols); sfx(SFX_HOVER); nav=true; }
  if(A.wheel && mouse_in(gx-8,gy-6,cols*cw+12,288)) cx_scroll-=A.wheel;
  if(nav){ int r=cx_sel/cols; if(r<cx_scroll) cx_scroll=r; if(r>=cx_scroll+vis) cx_scroll=r-vis+1; }
  cx_scroll=clampi(cx_scroll,0,maxi(0,rows-vis));
  if(rows>vis){ int sx=gx+cols*cw+1, sh=vis*ch-4, th=maxi(12,sh*vis/rows), ty=gy+(sh-th)*cx_scroll/maxi(1,rows-vis);
    gfx_rect(sx,gy,3,sh,HEX(0x1e1828)); gfx_rect(sx,ty,3,th,HEX(0x8a7a5a)); }
  for(int i=cx_scroll*cols;i<n && i<(cx_scroll+vis)*cols;i++){
    int x=gx+(i%cols)*cw, y=gy+(i/cols-cx_scroll)*ch; bool sel=i==cx_sel; bool kn=cx_known(cx_tab,i);
    if(mouse_in(x,y,cw-4,ch-4) && (A.mouse_moved||A.click)) cx_sel=i;
    gfx_rect(x,y,cw-4,ch-4,sel?HEX(0x2a2038):HEX(0x14101a)); gfx_frame(x,y,cw-4,ch-4,sel?HEX(0xd8b060):HEX(0x2e2638));
    if(cx_tab==2 && IS_EVO(BK_FIRST_BASE+i)) gfx_rect(x+1,y+1,cw-6,1,HEX(0x9a6ad8));
    cx_cell(cx_tab,i,x,y,kn);
  }
  /* fiche */
  int dx=gx+cols*cw+16, dw=SCREEN_W-dx-12; ui_fancy_panel(dx,50,dw,288);
  bool kn=cx_known(cx_tab,cx_sel); char b[256]; int y=60;
  if(cx_tab==0){
    const EnemyDef *d=&ENEMIES[cx_sel];
    text_draw(dx+10,y,kn?d->name:"???",HEX(0xffd84a),1,AL_LEFT,TX_SHADOW); y+=16;
    if(kn){ if(d->sprite>=0){ DrawOpt o=dopt(); const Sprite *s=&SPR[d->sprite]; o.sx=o.sy=SPR_LO[d->sprite].w>18?1.0f:2.0f; draw_spr_c(s,(float)dx+dw/2,(float)y+30,&o); } else art_draw_tentacle((float)dx+dw/2,(float)y+56,g_time,40,HEX(0xa35fe0),HEX(0x4a2266),-PI*0.5f); }
    y+=66;
    if(kn){
      char tr[160]; enemy_traits(cx_sel,tr,sizeof tr); snprintf(b,sizeof b,"^7%s",tr); y+=text_wrap(dx+10,y,dw-20,b,HEX(0xb0a8c0),1,0,11)+4;
      y+=text_wrap(dx+10,y,dw-20,d->lore,HEX(0xd8d0e4),1,0,11)+6;
      snprintf(b,sizeof b,"Points de vie : ^1%d",(int)(d->hp*EN_HP)); text_draw(dx+10,y,b,HEX(0x9a94a8),1,AL_LEFT,0); y+=12;
      snprintf(b,sizeof b,"Vaincus : ^1%d",S.kills_en[cx_sel]); text_draw(dx+10,y,b,HEX(0x9a94a8),1,AL_LEFT,0); y+=12;
      { int bt=bestiary_tier(cx_sel); snprintf(b,sizeof b,"Connaissance : ^2%d / 3^0%s",bt,bt>=2?(bt>=3?"  (^1+10 %%^0 dégâts)":"  (^1+5 %%^0 dégâts)"):""); text_draw(dx+10,y,b,HEX(0x9a94a8),1,AL_LEFT,0); y+=12;
        if(bt<3){ snprintf(b,sizeof b,"^7Niveau %d à %d victimes",bt+1,bestiary_need(cx_sel,bt+1)); text_draw(dx+10,y,b,HEX(0x8a82a0),1,AL_LEFT,0); } }
    } else text_wrap(dx+10,y,dw-20,"Vainquez ce monstre dans le gouffre pour l'ajouter au codex.",HEX(0x8a82a0),1,0,11);
  } else if(cx_tab==1){
    text_draw(dx+10,y,kn?BOSSES[cx_sel].name:"???",HEX(0xffd84a),1,AL_LEFT,TX_SHADOW); y+=18;
    if(kn){ y+=text_wrap(dx+10,y,dw-20,BOSSES[cx_sel].lore,HEX(0xd8d0e4),1,0,11)+8;
      snprintf(b,sizeof b,"Gardien du niveau ^1%d^0 : %s",cx_sel+1,BIOMES[cx_sel].name); text_draw(dx+10,y,b,HEX(0x9a94a8),1,AL_LEFT,0); y+=12;
      { static const int SH[BOSS_COUNT]={2600,3300,4300,5200,5600,6800,8000,9500,5800,7000,8200,10000}; snprintf(b,sizeof b,"Points de vie : ^1%d",SH[cx_sel]); text_draw(dx+10,y,b,HEX(0x9a94a8),1,AL_LEFT,0); y+=12; }
      snprintf(b,sizeof b,"Vaincu : ^1%d fois",S.boss_kills[cx_sel]); text_draw(dx+10,y,b,HEX(0x9a94a8),1,AL_LEFT,0); y+=12;
      { int bt=boss_tier(cx_sel); snprintf(b,sizeof b,"Connaissance : ^2%d / 3^0%s",bt,bt>=2?(bt>=3?"  (^1+10 %%^0 dégâts)":"  (^1+5 %%^0 dégâts)"):""); text_draw(dx+10,y,b,HEX(0x9a94a8),1,AL_LEFT,0); y+=12;
        if(bt<3){ snprintf(b,sizeof b,"^7Niveau %d à %d victoires",bt+1,bt+1>=3?20:5); text_draw(dx+10,y,b,HEX(0x8a82a0),1,AL_LEFT,0); } } }
    else text_wrap(dx+10,y,dw-20,"Un gardien encore invaincu garde ce niveau.",HEX(0x8a82a0),1,0,11);
  } else if(cx_tab==2){
    int k=BK_FIRST_BASE+cx_sel;
    text_draw(dx+10,y,kn?BALLS[k].name:"???",IS_EVO(k)?HEX(0xd2b0f5):HEX(0xffd84a),1,AL_LEFT,TX_SHADOW); y+=16;
    DrawOpt o=dopt(); if(!kn){ o.flash=HEX(0x241e2c); o.flash_a=255; } icon_draw(k,(float)dx+dw/2,(float)y+24,52,&o); y+=54;
    if(IS_EVO(k)){ snprintf(b,sizeof b,IS_LEG(k)?"Fusion légendaire : ^6%s^0 + ^6%s":"Fusion : ^6%s^0 + ^6%s",BALLS[BALLS[k].evo_a].name,BALLS[BALLS[k].evo_b].name); y+=text_wrap(dx+10,y,dw-20,b,HEX(0xb0a8c0),1,0,11)+4; }
    if(kn){ ball_desc(k,1,b,sizeof b); y+=text_wrap(dx+10,y,dw-20,b,HEX(0xd8d0e4),1,0,11)+8;
      if(ball_masterable(k)){ int st=ball_stars(k); snprintf(b,sizeof b,"Maîtrise : ^1%d / 3^0  (^1+%d %%^0 dégâts)",st,4*st); text_draw(dx+10,y,b,HEX(0x9a94a8),1,AL_LEFT,0); y+=12;
      if(st<3) snprintf(b,sizeof b,"^7Victimes : %d / %d",S.ball_kills[k],ball_star_need(k,st+1)); else snprintf(b,sizeof b,"^2Bille maîtrisée^0 (%d victimes)",S.ball_kills[k]);
      text_draw(dx+10,y,b,HEX(0x8a82a0),1,AL_LEFT,0); }
      else { snprintf(b,sizeof b,"^7Victimes : %d",S.ball_kills[k]); text_draw(dx+10,y,b,HEX(0x8a82a0),1,AL_LEFT,0); } }
    else if(IS_LEG(k)) text_wrap(dx+10,y,dw-20,"Construisez le Creuset légendaire, puis réunissez ces deux évolutions dans une même descente pour réaliser cette fusion.",HEX(0x8a82a0),1,0,11);
    else if(IS_EVO(k)) text_wrap(dx+10,y,dw-20,"Montez les deux billes au niveau 3 dans une même descente pour découvrir cette évolution.",HEX(0x8a82a0),1,0,11);
    else { int a=ach_for_reward(RW_BALL,k); snprintf(b,sizeof b,"Se débloque avec le défi ^1%s^0 : %s",a>=0?ACHS[a].name:"?",a>=0?ACHS[a].desc:""); text_wrap(dx+10,y,dw-20,b,HEX(0x8a82a0),1,0,11); }
  } else {
    text_draw(dx+10,y,kn?PASSIVES[cx_sel].name:"???",HEX(0xffd84a),1,AL_LEFT,TX_SHADOW); y+=16;
    DrawOpt o=dopt(); if(!kn){ o.flash=HEX(0x241e2c); o.flash_a=255; } icon_draw(ICON_PAS(cx_sel),(float)dx+dw/2,(float)y+24,52,&o); y+=54;
    if(kn){ for(int l=1;l<=3;l++){ char d2[160]; passive_desc(cx_sel,l,d2,sizeof d2); snprintf(b,sizeof b,"^7Niv. %d :^0 %s",l,d2); y+=text_wrap(dx+10,y,dw-20,b,HEX(0xd8d0e4),1,0,11)+3; } }
    else { int a=ach_for_reward(RW_PAS,cx_sel); snprintf(b,sizeof b,"Se débloque avec le défi ^1%s^0 : %s",a>=0?ACHS[a].name:"?",a>=0?ACHS[a].desc:""); text_wrap(dx+10,y,dw-20,b,HEX(0x8a82a0),1,0,11); }
  }
  (void)known_total;
}
static void journal_detail(int c,char *b,int n){
  switch(c){
    case 0: snprintf(b,n,"%d / %d défis accomplis",meta_new_count(),ACH_COUNT); break;
    case 1: { int t=0; for(int L=0;L<BIOME_COUNT;L++) t+=lvl_maxwon(L); snprintf(b,n,"%d / %d profondeurs gagnées    %d / %d coches de héros",t,BIOME_COUNT*DEPTH_MAX,lvl_checks(),BIOME_COUNT*CH_COUNT); } break;
    case 2: { int t=0, r10=0, tt=0; for(int h=0;h<CH_COUNT;h++){ t+=hero_rank(h)-1; r10+=hero_rank(h)>=RANK_MAX; tt+=train_total(h); } snprintf(b,n,"%d / %d rangs gagnés    %d héros au rang 10    entraînement : %d niveau%s",t,CH_COUNT*(RANK_MAX-1),r10,tt,tt>1?"x":""); } break;
    case 3: snprintf(b,n,"%d / %d étoiles de maîtrise",total_stars(),STARS_MAX); break;
    case 4: { int t=0; for(int k=0;k<EK_COUNT;k++) t+=bestiary_tier(k); for(int q=0;q<BOSS_COUNT;q++) t+=boss_tier(q); snprintf(b,n,"%d / %d niveaux de connaissance (monstres et gardiens)",t,3*(EK_COUNT+BOSS_COUNT)); } break;
    case 5: { int t=0,m=0; for(int q=0;q<BLD_COUNT;q++){ t+=S.bld[q]; m+=BUILDINGS[q].max; } snprintf(b,n,"%d / %d niveaux de bâtiments",t,m); } break;
    case 6: { int h=0; for(int q=0;q<CH_COUNT;q++) h+=S.unlocked[q]; snprintf(b,n,"Héros %d/%d   billes et objets %d/%d   évolutions %d/%d",h,CH_COUNT,coll_count(),BASE_BALL_COUNT+PS_COUNT,popcount32(S.evo_seen&((1u<<EVO_COUNT)-1)),EVO_COUNT); } break;
    default: snprintf(b,n,"Record : étage %d   (objectif : étage 18)",S.endless_best); break;
  }
}
static void journal_frame(void){
  float pct=completion_pct(); char b[200];
  text_draw(SCREEN_W/2,8,"JOURNAL DE PROGRESSION",HEX(0xffd84a),2,AL_CENTER,TX_OUTLINE);
  snprintf(b,sizeof b,"%.1f %%",pct); for(char *q=b;*q;q++) if(*q=='.') *q=',';
  text_draw(SCREEN_W/2,30,b,pct>=99.95f?HEX(0xffd84a):HEX(0xffffff),3,AL_CENTER,TX_OUTLINE);
  ui_bar(120,64,400,6,pct/100.0f,HEX(0xffc040),HEX(0x1e1828));
  ui_fancy_panel(40,78,560,250);
  for(int c=0;c<COMP_CATS;c++){
    float w; const char *nm; float f=clampf(completion_part(c,&w,&nm),0,1);
    int y=86+c*28;
    snprintf(b,sizeof b,"%s  ^7(%d %%)",nm,(int)w); text_draw(54,y,b,f>=0.9999f?HEX(0xffd84a):HEX(0xf0e6d0),1,AL_LEFT,TX_SHADOW);
    snprintf(b,sizeof b,"%d %%",(int)(f*100+0.0001f)); text_draw(586,y,b,f>=0.9999f?HEX(0x8be05a):HEX(0xd8d0e4),1,AL_RIGHT,0);
    ui_bar(300,y+3,240,5,f,f>=0.9999f?HEX(0x8be05a):HEX(0x5ab8ff),HEX(0x1e1828));
    journal_detail(c,b,sizeof b); text_draw(54,y+12,b,HEX(0x8a82a0),1,AL_LEFT,0);
  }
  /* prochains defis : les plus proches d'etre accomplis */
  { int best[3]={-1,-1,-1}; float bv[3]={-1,-1,-1};
    for(int i=0;i<ACH_COUNT;i++){ if(S.ach[i]) continue; int tg; int v=ach_progress(i,&tg); if(tg<=1) continue; float f=(float)v/tg;
      for(int k=0;k<3;k++) if(f>bv[k]){ for(int q=2;q>k;q--){ bv[q]=bv[q-1]; best[q]=best[q-1]; } bv[k]=f; best[k]=i; break; } }
    char nb[200]; nb[0]=0;
    for(int k=0;k<3;k++){ if(best[k]<0) break; size_t l=strlen(nb); snprintf(nb+l,sizeof nb-l,"%s%s ^1%d %%^0",l?"   ":"",ACHS[best[k]].name,(int)(bv[k]*100)); }
    if(nb[0]){ snprintf(b,sizeof b,"^7Prochains défis :^0 %s",nb); text_draw(54,311,b,HEX(0xd8d0e4),1,AL_LEFT,0); } }
  int h=S.play_time/3600, m=(S.play_time/60)%60;
  snprintf(b,sizeof b,"Temps en descente : ^1%d h %02d^0     Descentes : ^1%d^0     Victoires : ^1%d",h,m,S.runs,S.wins);
  text_draw(SCREEN_W/2,334,b,HEX(0xb0a8c0),1,AL_CENTER,0);
}
void meta_frame(float dt){
  menu_bg_public(dt,36);
  gfx_darken(0,0,SCREEN_W,SCREEN_H,150);
  if(g_gpu){ GP.blur_all=0.6f; layer(LY_UI); }
  if(meta_scr==META_ACH) ach_frame(); else if(meta_scr==META_JOURNAL) journal_frame(); else codex_frame();
  char b[160];
  if(meta_scr==META_JOURNAL){ snprintf(b,sizeof b,A.pad?"%s : retour":"Échap : retour",btn(B_BACK)); text_draw(SCREEN_W/2,SCREEN_H-14,b,HEX(0x8a82a0),1,AL_CENTER,0); }
  else {
  if(A.pad) snprintf(b,sizeof b,meta_scr==META_ACH?"Haut / Bas : parcourir     %s : retour":"Croix : parcourir   L1 / R1 : onglets   %s : retour",btn(B_BACK));
  else snprintf(b,sizeof b,meta_scr==META_ACH?"Flèches / molette : parcourir     Échap : retour":"Flèches : parcourir   Tab : onglet suivant   Échap : retour");
  text_draw(SCREEN_W/2,SCREEN_H-14,b,HEX(0x8a82a0),1,AL_CENTER,0); }
  if(A.back || A.rclick){ goto_state(meta_ret); sfx(SFX_CLICK); }
}

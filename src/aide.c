/* GOUFFRE - v9.1 : menu « Aide » (titre, campement, pause) : rubriques sur les regles, les billes,
   les niveaux, le campement, les modes de jeu et les commandes. */
#include "game.h"

static int help_ret=GS_TITLE, help_page=0; static float help_in=0;
void help_open(int ret){ help_ret=ret; help_page=0; help_in=0; ui_set_focus(0); goto_state(GS_HELP); }

typedef struct { const char *title; const char *text; } HelpPage;
static const HelpPage HP[]={
 {"Le but",
  "Des monstres descendent vers le fond du gouffre. ^1Ne laissez rien atteindre le fond !^0\n\n"
  "Vos ^1billes^0 partent toutes seules dans la direction visée, rebondissent sur les murs et les monstres, "
  "puis reviennent au sol où vous les rattrapez. Déplacez-vous pour les relancer plus vite.\n\n"
  "Les monstres vaincus lâchent des ^4gemmes d'expérience^0 et de l'^1or^0. À chaque niveau d'expérience, "
  "choisissez une carte : nouvelle bille, bille améliorée, objet passif ou évolution.\n\n"
  "Chaque monstre qui passe vous coûte des points de vie. À zéro, la descente est finie : "
  "l'or et les cristaux gagnés restent acquis pour le campement."},
 {"Les billes",
  "Vous emportez jusqu'à ^15 billes^0 et ^15 objets passifs^0.\n\n"
  "Chaque bille a son effet : feu (brûlure), glace (gel), venin, foudre, rebonds, perforation, "
  "ombre (marque les monstres), lave (flaques), étoilée (étoiles filantes), boomerang (frappe à l'aller "
  "et au retour), sismique (ondes de choc le long des murs), chaîne (deux monstres partagent les dégâts)...\n\n"
  "Améliorer une bille augmente ses dégâts et son effet. Les objets passifs améliorent le héros : "
  "vitesse, vie, coups critiques, aimant à gemmes...\n\n"
  "^7Astuce :^0 visez les trous entre les monstres. Une bille coincée au-dessus d'eux rebondit "
  "sans arrêt et fait des ravages.\n\n"
  "Pendant un choix, ^1E^0 (ou la gâchette) ouvre l'encyclopédie et la fiche détaillée."},
 {"Évolutions et fusions",
  "^6Évolution :^0 deux billes qui vont ensemble, toutes deux au ^1niveau 3^0, peuvent fusionner. "
  "Une carte « Évolution » apparaît alors parmi les choix. La fiche d'une bille montre ses synergies.\n\n"
  "Exemples : Ombre + Lumière = Éclipse, Lave + Bombe = Volcan, Boomerang + Épine = Chakram, "
  "Sismique + Plomb = Faille, Chaîne + Aimant = Entrave.\n\n"
  "^6Fusion légendaire :^0 construisez le ^1Creuset légendaire^0 au campement. Quand vous possédez "
  "deux évolutions qui vont ensemble, une carte rare propose de les fondre en une bille légendaire :\n"
  "  Singularité + Supernova = Big Bang\n  Rayon + Tonnerre = Jugement dernier\n"
  "  Blizzard + Vapeur = Zéro absolu\n  Faucheuse + Fléau = Ouroboros\n  Faille + Volcan = Apocalypse\n\n"
  "Les évolutions découvertes s'ajoutent au Codex."},
 {"Niveaux et gardiens",
  "12 niveaux en trois destinations : le ^1Gouffre^0, les ^4Profondeurs^0 et le ^5Royaume oublié^0. "
  "Chaque niveau dure environ 15 minutes.\n\n"
  "Au tiers et aux deux tiers du niveau arrive un ^1gardien mineur^0 ; le décor change après chacun. "
  "À la fin, le ^3gardien final^0 vous attend dans son arène.\n\n"
  "Gagner un niveau débloque le suivant et sa ^1profondeur^0 suivante, plus difficile et mieux récompensée "
  "(L1 / R1 ou flèches dans la liste des niveaux).\n\n"
  "Chaque héros qui termine un niveau y gagne une ^2coche^0. Un compagnon peut vous accompagner : "
  "deux talents valent mieux qu'un.\n\n"
  "Trop dur ? Options > Difficulté (Facile, Normale, Difficile), même en pleine descente."},
 {"Curiosités et objectifs",
  "^1Curiosités :^0 toutes les 20 à 30 s, un objet apparaît sur le sol et descend avec lui. "
  "Les ^2trésors^0 se brisent en quelques coups (or, expérience, parfois un cœur). Les ^3pièges^0 "
  "éclatent au premier coup et frappent les monstres autour (feu, poison, gel, éclairs...) : "
  "ils ne blessent jamais le héros, visez-les quand une troupe passe tout près.\n\n"
  "^1Objectifs :^0 chaque niveau propose 3 objectifs (briser des curiosités, vaincre le gardien vite, "
  "laisser passer peu de monstres, vaincre des champions, atteindre un combo...). Chaque objectif "
  "rempli à la victoire donne une ^1étoile^0 et ^62 cristaux^0.\n\n"
  "Pendant une descente, la pause (onglet ^1Objectifs^0) montre où vous en êtes."},
 {"Champions",
  "Les ^1champions^0 sont des monstres plus forts, entourés d'un halo de couleur, avec leur pouvoir écrit "
  "au-dessus d'eux. Ils lâchent beaucoup d'or et d'expérience. Ils apparaissent dans les profondeurs 2 et plus, "
  "et dans les derniers niveaux :\n"
  "  Rapide : descend bien plus vite\n  Colosse : énorme et très résistant\n"
  "  Régénérant : guérit quand on le laisse tranquille\n  Bouclier : un bouclier absorbe les premiers coups\n"
  "  Explosif : en mourant, projette une couronne de tirs\n  Invocateur : appelle des renforts\n"
  "  Soigneur : soigne les monstres autour de lui\n  Spectral : devient intouchable un instant toutes les 3 s\n\n"
  "^7Astuce :^0 la Loupe et la bille d'ombre aident beaucoup contre eux."},
 {"Mutateurs",
  "Les ^1mutateurs^0 (défi « Apprenti sorcier ») ajoutent des règles aux niveaux : monstres plus rapides, "
  "horde, aucun soin, gardiens enragés, élites renforcées, canon de verre...\n\n"
  "Dans la liste des niveaux, appuyez sur ^1M^0 (ou X / Carré) pour les cocher. Chaque mutateur donne "
  "^1plus d'or^0 et ^4plus d'expérience^0 pendant le niveau, et ^6un cristal^0 de plus à la victoire.\n\n"
  "Ils restent cochés d'un niveau à l'autre : pensez à les retirer avant un niveau difficile !\n\n"
  "Défis : gagner avec 1, 4, 8 puis les 12 mutateurs à la fois."},
 {"Campement et héros",
  "L'^1or^0 et les ^6cristaux^0 servent au campement (menu ^1Amélioration^0) : la Forge (dégâts), "
  "l'Infirmerie (vie), le Laboratoire, l'Observatoire, le Creuset légendaire, le Cabinet des curiosités...\n\n"
  "Les ^1prismes^0 gagnés sur les gardiens entraînent vos héros (menu ^1Entraînement^0) : "
  "les premiers niveaux coûtent peu, répartissez-les.\n\n"
  "Chaque héros a sa bille de départ, son talent et ses rangs : jouer un héros le fait monter "
  "de rang et débloque son talent du rang 5.\n\n"
  "Les nouveaux héros se débloquent par les ^1Défis^0 (L'Astronome : « Constellations », "
  "la Voleuse d'ombres : « Monte-en-l'air », la Chasseresse : « La traque », le Géomancien : « Fondations »...). Le ^1Journal^0 montre ce qu'il reste pour atteindre 100 %."},
 {"Modes de jeu",
  "Dans la liste des niveaux, ^1Tab^0 (ou L2 / R2) ouvre la page des modes :\n\n"
  "^1Infini^0 : des étages courts qui s'enchaînent sans fin, de plus en plus durs.\n\n"
  "^1Défi du jour^0 : un niveau, un héros et des règles imposés, les mêmes pour tout le monde ce jour-là.\n\n"
  "^3Rush des gardiens^0 : les 4 gardiens d'une destination à la suite, directement dans leurs arènes, "
  "avec un record de temps. Choisissez la destination avec les flèches.\n\n"
  "Ces modes se débloquent par les défis « Habitué », « Purificateur » et « Gladiateur ».\n\n"
  "Pendant une descente, ^1R^0 (ou R1) change la vitesse du jeu : x1, x1,5, x2."},
 {"Manette",
  "Stick gauche / croix : se déplacer, naviguer\n"
  "Stick droit : viser (relâché = visée automatique)\n"
  "A / Croix : valider        B / Rond : retour\n"
  "Y / Triangle : changer de visée, relancer les choix,\n"
  "      menu Amélioration (campement)\n"
  "X / Carré : passer un choix, menu Entraînement\n"
  "L1 / R1 : onglets, profondeur ; R1 en partie : vitesse\n"
  "L2 / R2 : page des modes, encyclopédie / détails\n"
  "Start / Options : pause\n\n"
  "Manettes Xbox, PS5 DualSense, PS4, Nintendo et génériques, en USB ou Bluetooth. "
  "Options > « Tester la manette » montre les boutons détectés."},
 {"Clavier et souris",
  "ZQSD ou flèches : se déplacer        Souris : viser\n"
  "Espace : visée automatique\n"
  "1, 2, 3 ou clic : choisir une carte\n"
  "R : relancer les choix / vitesse du jeu\n"
  "Retour arrière : passer un choix\n"
  "E : encyclopédie (pendant un choix)\n"
  "Tab : page, onglet, détails\n"
  "Échap : pause / retour\n"
  "F11 ou Alt+Entrée : plein écran\n\n"
  "Campement : flèches ou clic pour choisir un bâtiment, R : Amélioration, T : Entraînement, "
  "E : expédition."},
 {"Astuces",
  "- Bloqué ? Passez en Facile le temps de renforcer la Forge et l'Infirmerie.\n"
  "- Contre les monstres qui se régénèrent : feu et venin empêchent la régénération.\n"
  "- Une bille d'ombre en premier : toutes les autres font plus de dégâts sur la cible marquée.\n"
  "- Gardez de la place pour les évolutions : deux billes qui vont ensemble valent mieux "
  "que cinq billes isolées.\n"
  "- Les pièges des curiosités sont des bombes gratuites : attendez qu'une troupe passe.\n"
  "- Variez les billes : les étoiles de maîtrise se gagnent bille par bille.\n"
  "- Ordinateur qui peine ? Options > Graphismes « Performance », ou « Lueurs : non ».\n"
  "- Le son : Options > Musique, Effets sonores et Ambiances."},
};
#define HP_N ((int)(sizeof HP/sizeof HP[0]))
/* sur iPhone, la page clavier devient la page tactile */
static const HelpPage HP_TOUCH={"Écran tactile",
  "Pouce gauche (moitié gauche) : se déplacer, le stick apparaît sous le doigt\n"
  "Pouce droit (moitié droite) : viser, en glissant dans la direction voulue\n"
  "Toucher brièvement : appuyer sur un bouton du jeu\n\n"
  "Boutons sur les côtés de l'écran :\n"
  "Pause / Échap : pause, ou retour dans les menus\n"
  "Vitesse : vitesse du jeu (x1, x1,5, x2)\n"
  "Visée : visée automatique ou au doigt\n\n"
  "Menus : touchez pour choisir, glissez pour faire défiler les listes. "
  "Cinématiques : maintenez le doigt pour passer.\n\n"
  "Une manette (DualSense, Xbox...) reste le plus confortable : elle prend le relais dès qu'on la touche."};
static const HelpPage *hp_page(int i){ return (A.touch && !strcmp(HP[i].title,"Clavier et souris")) ? &HP_TOUCH : &HP[i]; }

void help_frame(float dt){
  if(help_ret==GS_RUN){ run_draw(); layer(LY_UI); gfx_darken(0,0,SCREEN_W,SCREEN_H,160); if(g_gpu){ GP.blur_all=0.85f; GP.desat=0.3f; } }
  else { menu_backdrop(dt,40); if(g_gpu){ GP.blur_all=0.6f; layer(LY_UI); } }
  help_in+=dt;
  int px=20, py=12, pw=600, ph=336; orn_panel(px,py,pw,ph);
  gothic_title(SCREEN_W/2,py+8,"Aide",24);
  if(A.up){ help_page=(help_page+HP_N-1)%HP_N; sfx(SFX_HOVER); }
  if(A.down||A.tab_r){ help_page=(help_page+1)%HP_N; sfx(SFX_HOVER); }
  if(A.tab_l){ help_page=(help_page+HP_N-1)%HP_N; sfx(SFX_HOVER); }
  /* rubriques */
  int lx=px+14, ly=py+38, lw=150, lh=20;
  for(int i=0;i<HP_N;i++){ int y=ly+i*(lh+2); bool hov=mouse_in(lx,y,lw,lh);
    if(hov && (A.mouse_moved||A.click) && help_page!=i){ help_page=i; sfx(SFX_HOVER); }
    for(int j=0;j<lh;j++) gfx_hline(lx,lx+lw-1,y+j,col_lerp(i==help_page?HEX(0x5a3440):(hov?HEX(0x3a2630):HEX(0x2a1820)),HEX(0x160c12),(float)j/lh));
    orn_frame(lx,y,lw,lh,i==help_page?1:0);
    text_draw(lx+8,y+(lh-TXT_H)/2+1,hp_page(i)->title,i==help_page?HEX(0xfff4ec):HEX(0xc8b8c0),1,AL_LEFT,TX_SHADOW); }
  /* texte */
  int tx=lx+lw+16, ty=py+42, tw=px+pw-18-tx;
  const HelpPage *p=hp_page(help_page);
  text_draw(tx,ty,p->title,HEX(0xffd84a),2,AL_LEFT,TX_OUTLINE);
  gfx_hline(tx,tx+tw,ty+20,HEX(0x8a5a2e)); gfx_hline(tx,tx+tw,ty+21,HEX(0x2a160c));
  text_wrap(tx,ty+30,tw,p->text,HEX(0xe8dce4),1,TX_SHADOW,11);
  /* pied */
  char b[160];
  if(A.pad) snprintf(b,sizeof b,"Haut / Bas : rubrique    %s : fermer",btn(B_BACK));
  else snprintf(b,sizeof b,"Haut / Bas ou clic : rubrique    Échap : fermer");
  text_draw(px+pw/2,py+ph-16,b,HEX(0x8a82a0),1,AL_CENTER,0);
  bool close=orn_button(px+pw-96,py+ph-24,80,18,"Fermer",false,true);
  if(help_in>0.15f && (close || A.back || A.pause)){ sfx(SFX_CLICK); goto_state(help_ret); }
}

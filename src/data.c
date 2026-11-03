/* GOUFFRE - donnees de jeu */
#include "game.h"

const BallDef BALLS[BK_COUNT] = {
  [BK_BABY]       = {"Petite bille",    HEX(0xe8e4f0),HEX(0x8e8898),2.0f, 6, 0,1.00f,-1,-1},
  [BK_FER]        = {"Bille de fer",    HEX(0xd8dcea),HEX(0x6a6e80),3.0f,18, 8,1.00f,-1,-1},
  [BK_FEU]        = {"Bille de feu",    HEX(0xffa030),HEX(0xc0401a),3.0f, 9, 3,1.00f,-1,-1},
  [BK_GLACE]      = {"Bille de givre",  HEX(0x9ae8ff),HEX(0x3a8ad8),3.0f, 9, 3,1.00f,-1,-1},
  [BK_FOUDRE]     = {"Bille de foudre", HEX(0xfff070),HEX(0xc89a20),3.0f, 8, 3,1.05f,-1,-1},
  [BK_VENIN]      = {"Bille de venin",  HEX(0x9af05a),HEX(0x3a8a3a),3.0f, 7, 2,1.00f,-1,-1},
  [BK_EPINE]      = {"Bille d'épines",  HEX(0xff5a6a),HEX(0x8a1a3a),3.0f, 9, 3,1.00f,-1,-1},
  [BK_BOMBE]      = {"Bille explosive", HEX(0xff7a3a),HEX(0x5a2a2a),3.5f,10, 4,0.95f,-1,-1},
  [BK_SPECTRE]    = {"Bille spectrale", HEX(0xd8c0ff),HEX(0x7a5ab8),3.0f,10, 4,1.00f,-1,-1},
  [BK_SANG]       = {"Bille vampire",   HEX(0xe84a7a),HEX(0x5a0a2a),3.0f,11, 4,1.00f,-1,-1},
  [BK_ECLAT]      = {"Bille d'éclats",  HEX(0xffe0f8),HEX(0xb87ad8),3.0f, 8, 3,1.00f,-1,-1},
  [BK_PLOMB]      = {"Bille de plomb",  HEX(0x9a9ab4),HEX(0x3a3a4a),5.0f,26,11,0.80f,-1,-1},
  [BK_AIMANT]     = {"Bille aimantée",  HEX(0x5a9aff),HEX(0x2a3a8a),3.0f,11, 4,1.00f,-1,-1},
  [BK_LUMIERE]    = {"Bille de lumière",HEX(0xfff4c0),HEX(0xc8a040),3.0f, 9, 3,1.05f,-1,-1},
  [BK_FOLLE]      = {"Bille folle",     HEX(0xff6ad0),HEX(0x8a2a8a),3.0f, 7, 3,1.15f,-1,-1},
  [BK_CRISTAL]    = {"Bille de cristal",HEX(0x9af8ff),HEX(0x3a9ab8),3.0f, 8, 3,1.00f,-1,-1},
  [BK_RAFALE]     = {"Bille du vent",   HEX(0xe0f8e8),HEX(0x6ac898),3.0f, 7, 3,1.10f,-1,-1},
  [BK_ECHO]       = {"Bille d'écho",    HEX(0xa898ff),HEX(0x3a2e8a),3.0f, 8, 3,1.00f,-1,-1},
  [BK_DOREE]      = {"Bille dorée",     HEX(0xffcc40),HEX(0x9a6a10),3.0f, 9, 3,1.00f,-1,-1},
  [BK_RACINE]     = {"Bille de racines",HEX(0xc89a5a),HEX(0x4a6a1a),3.0f, 9, 3,1.00f,-1,-1},
  [BK_ACIDE]      = {"Bille d'acide",   HEX(0xd8ff40),HEX(0x5a7a10),3.0f, 8, 3,1.00f,-1,-1},
  [BK_OMBRE]      = {"Bille d'ombre",   HEX(0x9a7ad8),HEX(0x24143a),3.0f, 9, 3,1.00f,-1,-1},
  [BK_RICOCHET]   = {"Bille de ricochet",HEX(0xf0a060),HEX(0x7a3a1a),3.0f, 8, 3,1.08f,-1,-1},
  [BK_LAVE]       = {"Bille de lave",   HEX(0xff7a30),HEX(0x5a1a0a),3.2f, 9, 3,0.97f,-1,-1},
  [BK_ETOILE]     = {"Bille étoilée",   HEX(0xb8c8ff),HEX(0x2a2a7a),3.0f, 8, 3,1.02f,-1,-1},
  [BK_BOOMERANG]  = {"Boomerang",       HEX(0xe0b070),HEX(0x6a3a1a),3.2f, 7, 3,1.05f,-1,-1},
  [BK_SEISME]     = {"Bille sismique",  HEX(0xd0a470),HEX(0x4a3020),3.4f, 9, 3,0.95f,-1,-1},
  [BK_CHAINE]     = {"Bille de chaîne", HEX(0xc0c8d8),HEX(0x3a4050),3.0f, 8, 3,1.00f,-1,-1},
  [BK_VAPEUR]     = {"Vapeur",          HEX(0xb8e4f4),HEX(0x4a6a8a),4.0f,24, 0,1.00f,BK_FEU,BK_GLACE},
  [BK_METEORE]    = {"Météore",         HEX(0xffc040),HEX(0xd0301a),4.5f,34, 0,1.00f,BK_FEU,BK_BOMBE},
  [BK_TEMPETE]    = {"Tempête",         HEX(0x6ae0ff),HEX(0x2a3ad0),4.0f,20, 0,1.10f,BK_FOUDRE,BK_GLACE},
  [BK_FLEAU]      = {"Fléau",           HEX(0xb8f04a),HEX(0x4a1a6a),4.0f,18, 0,1.00f,BK_VENIN,BK_EPINE},
  [BK_FAUCHEUSE]  = {"Faucheuse",       HEX(0xff8ab0),HEX(0x5a1a4a),4.0f,30, 0,1.05f,BK_SPECTRE,BK_SANG},
  [BK_PRISME]     = {"Prisme",          HEX(0xffffff),HEX(0x9a7af0),4.0f,20, 0,1.05f,BK_ECLAT,BK_FOUDRE},
  [BK_SINGULARITE]= {"Singularité",     HEX(0x8a5aff),HEX(0x1a0a3a),5.5f,44, 0,0.90f,BK_PLOMB,BK_AIMANT},
  [BK_ENCLUME]    = {"Enclume",         HEX(0xf0f0ff),HEX(0x4a4a6a),5.5f,70, 0,0.90f,BK_FER,BK_PLOMB},
  [BK_RAYON]      = {"Rayon divin",     HEX(0xfff8e0),HEX(0xffc040),4.0f,22, 0,1.05f,BK_LUMIERE,BK_SPECTRE},
  [BK_KALEIDO]    = {"Kaléidoscope",    HEX(0xffffff),HEX(0xc06aff),4.0f,15, 0,1.15f,BK_FOLLE,BK_ECLAT},
  [BK_GEODE]      = {"Géode toxique",   HEX(0xd090ff),HEX(0x5a2a8a),4.5f,20, 0,1.00f,BK_CRISTAL,BK_VENIN},
  [BK_BLIZZARD]   = {"Blizzard",        HEX(0xe8f8ff),HEX(0x5ab8ff),4.0f,18, 0,1.10f,BK_RAFALE,BK_GLACE},
  [BK_TEMPFEU]    = {"Tempête de feu",  HEX(0xffa050),HEX(0xc0301a),4.0f,18, 0,1.10f,BK_RAFALE,BK_FEU},
  [BK_AURORE]     = {"Aurore",          HEX(0xffb8e8),HEX(0x6ad8e0),4.0f,24, 0,1.05f,BK_LUMIERE,BK_SANG},
  [BK_TONNERRE]   = {"Tonnerre",        HEX(0xd8d0ff),HEX(0x4a3ac8),4.0f,22, 0,1.05f,BK_ECHO,BK_FOUDRE},
  [BK_MIDAS]      = {"Midas",           HEX(0xffe070),HEX(0xb07a10),4.5f,26, 0,0.95f,BK_DOREE,BK_FER},
  [BK_RONCES]     = {"Ronces",          HEX(0xa8d060),HEX(0x5a2a1a),4.0f,18, 0,1.00f,BK_RACINE,BK_EPINE},
  [BK_PLUIE]      = {"Pluie acide",     HEX(0xe8ff70),HEX(0x3a6a10),4.5f,18, 0,1.00f,BK_ACIDE,BK_BOMBE},
  [BK_ECLIPSE]    = {"Éclipse",         HEX(0xfff0c0),HEX(0x1a1028),4.0f,26, 0,1.05f,BK_OMBRE,BK_LUMIERE},
  [BK_CAUCHEMAR]  = {"Cauchemar",       HEX(0xb06aff),HEX(0x12081e),4.0f,26, 0,1.00f,BK_OMBRE,BK_SPECTRE},
  [BK_CARAMBOLE]  = {"Carambole",       HEX(0xff6a5a),HEX(0x7a2a1a),4.0f,16, 0,1.15f,BK_RICOCHET,BK_FOLLE},
  [BK_VOLCAN]     = {"Volcan",          HEX(0xff8a2a),HEX(0x3a1208),4.8f,32, 0,0.95f,BK_LAVE,BK_BOMBE},
  [BK_CONSTELLATION]={"Constellation",  HEX(0xd8e0ff),HEX(0x3a3aa8),4.0f,20, 0,1.05f,BK_ETOILE,BK_CRISTAL},
  [BK_SUPERNOVA]  = {"Supernova",       HEX(0xfff0a0),HEX(0xd85a1a),4.5f,26, 0,1.00f,BK_ETOILE,BK_LAVE},
  [BK_CHAKRAM]    = {"Chakram",         HEX(0xffe0a0),HEX(0x8a2a1a),4.2f,20, 0,1.10f,BK_BOOMERANG,BK_EPINE},
  [BK_FAILLE]     = {"Faille",          HEX(0xe8b070),HEX(0x3a1a0a),5.0f,30, 0,0.95f,BK_SEISME,BK_PLOMB},
  [BK_ENTRAVE]    = {"Entrave",         HEX(0xd0d8f0),HEX(0x2a2a4a),4.2f,22, 0,1.00f,BK_CHAINE,BK_AIMANT},
  [BK_BIGBANG]    = {"Big Bang",        HEX(0xe8d8ff),HEX(0x2a0a4a),6.0f,60, 0,0.92f,BK_SINGULARITE,BK_SUPERNOVA},
  [BK_JUGEMENT]   = {"Jugement dernier",HEX(0xfff8d0),HEX(0x6a5ad8),5.0f,46, 0,1.05f,BK_RAYON,BK_TONNERRE},
  [BK_ZERO]       = {"Zéro absolu",     HEX(0xf0fcff),HEX(0x4aa8f0),5.0f,40, 0,1.05f,BK_BLIZZARD,BK_VAPEUR},
  [BK_OUROBOROS]  = {"Ouroboros",       HEX(0x9af06a),HEX(0x3a0a2a),5.0f,48, 0,1.00f,BK_FAUCHEUSE,BK_FLEAU},
  [BK_APOCALYPSE] = {"Apocalypse",      HEX(0xff7030),HEX(0x2a0808),6.0f,56, 0,0.92f,BK_FAILLE,BK_VOLCAN},
  [BK_SHARD]      = {"Éclat",           HEX(0xffe0f8),HEX(0xb87ad8),1.5f, 5, 0,1.20f,-1,-1},
  [BK_GHOST]      = {"Âme",             HEX(0x8affc0),HEX(0x2a7a5a),2.5f,12, 0,1.00f,-1,-1},
};

static int bdmg(int k,int lvl){ return (int)(BALLS[k].dmg + BALLS[k].dmg_lvl*(lvl-1)); }
/* sauvegardes d'avant la v7 : les evolutions et les billes temporaires sont passees de 4 rangs plus loin */
/* v9 : quatre billes de base inserees avant les evolutions */
int bk_v10_to_v11(int k){ if(k<0) return -1; if(k<=24) return k; if(k<=48) return k+3; if(k<=52) return k+6; if(k==53) return BK_SHARD; if(k==54) return BK_GHOST; return -1; }
uint32_t evo_seen_v10_to_v11(uint32_t m){ return (m&0xFFFFFFu) | (((m>>24)&0xFu)<<27); }
int bk_v9_to_v10(int k){ if(k<0) return -1; if(k<=BK_ACIDE) return k; if(k<=38) return k+4; if(k==39) return BK_SHARD; if(k==40) return BK_GHOST; return -1; }
int bk_old_to_new(int k){ if(k<0) return -1; if(k<=BK_RAFALE) return k; if(k<=30) return k+4; if(k==31) return BK_SHARD; if(k==32) return BK_GHOST; return -1; }

void ball_desc(int k,int lvl,char *b,int n){
  int d=bdmg(k,lvl);
  switch(k){
    case BK_FER: snprintf(b,n,"Lourde et fiable. Dégâts ^1%d^0.",d); break;
    case BK_FEU: snprintf(b,n,"Enflamme : ^5+%d brûlure%s^0 par coup (max ^5%d^0). Dégâts ^1%d^0.",lvl>=3?2:1,lvl>=3?"s":"",4+2*lvl,d); break;
    case BK_GLACE: snprintf(b,n,"Ralentit, et ^4%d%%^0 de chances de geler. Les ennemis gelés subissent ^4+30%%^0. Dégâts ^1%d^0.",8+7*lvl,d); break;
    case BK_FOUDRE: snprintf(b,n,"Un éclair rebondit sur ^1%d^0 ennemis proches (^1%d^0 dégâts). Dégâts ^1%d^0.",1+lvl,5+3*lvl,d); break;
    case BK_VENIN: snprintf(b,n,"Empoisonne : ^2+%d poison^0 par coup, cumulable sans limite. Dégâts ^1%d^0.",lvl>=2?2:1,d); break;
    case BK_EPINE: snprintf(b,n,"Fait saigner (^3max %d^0). Chaque coup sur un ennemi qui saigne fait ^3+2^0 par saignement. Dégâts ^1%d^0.",4+2*lvl,d); break;
    case BK_BOMBE: snprintf(b,n,"Explose à chaque impact : ^5%d^0 dégâts dans un rayon de ^5%d^0. Dégâts ^1%d^0.",6+4*lvl,16+4*lvl,d); break;
    case BK_SPECTRE: snprintf(b,n,"Traverse les ennemis au lieu de rebondir. Dégâts ^1%d^0.",d); break;
    case BK_SANG: snprintf(b,n,"^3%d%%^0 de chances de vous soigner de ^3"IC_HEART"1^0 à chaque coup. Dégâts ^1%d^0.",12+8*lvl,d); break;
    case BK_ECLAT: snprintf(b,n,"Se brise en ^6%d éclat%s^0 qui rebondissent partout. Dégâts ^1%d^0.",lvl,lvl>1?"s":"",d); break;
    case BK_PLOMB: snprintf(b,n,"Énorme et lente. Repousse les ennemis vers le haut. Dégâts ^1%d^0.",d); break;
    case BK_AIMANT: snprintf(b,n,"Attirée par les ennemis : sa trajectoire se courbe vers eux. Dégâts ^1%d^0.",d); break;
    case BK_LUMIERE: snprintf(b,n,"Traverse ^1%d^0 ennemi%s avant de rebondir et les ^1aveugle^0 (ils ne tirent plus). Dégâts ^1%d^0.",lvl,lvl>1?"s":"",d); break;
    case BK_FOLLE: snprintf(b,n,"Rebonds imprévisibles. ^6+%d%%^0 de dégâts à chaque rebond depuis le lancer. Dégâts ^1%d^0.",9+3*lvl,d); break;
    case BK_CRISTAL: snprintf(b,n,"^4+%d%%^0 de dégâts par altération sur la cible (feu, poison, saignement, gel, cécité). Dégâts ^1%d^0.",20+5*lvl,d); break;
    case BK_RAFALE: snprintf(b,n,"Chaque impact libère une ^2bourrasque^0 qui repousse et ralentit autour (^2%d^0 dégâts). Dégâts ^1%d^0.",3+2*lvl,d); break;
    case BK_VAPEUR: snprintf(b,n,"Chaque impact crée un nuage brûlant qui ralentit et inflige ^514^0/s. Dégâts ^1%d^0.",d); break;
    case BK_METEORE: snprintf(b,n,"Explosion géante (^530^0 dégâts) qui enflamme tout autour. Dégâts ^1%d^0.",d); break;
    case BK_TEMPETE: snprintf(b,n,"Éclairs en chaîne sur ^15^0 ennemis (^122^0) avec ^435%%^0 de gel. Dégâts ^1%d^0.",d); break;
    case BK_FLEAU: snprintf(b,n,"Poison et saignement ; le poison se ^2propage^0 quand la victime meurt. Dégâts ^1%d^0.",d); break;
    case BK_FAUCHEUSE: snprintf(b,n,"Traverse tout, ^3achève^0 les ennemis sous 18%% de vie et soigne. Dégâts ^1%d^0.",d); break;
    case BK_PRISME: snprintf(b,n,"Se brise en ^63 éclats^0 électriques qui foudroient. Dégâts ^1%d^0.",d); break;
    case BK_SINGULARITE: snprintf(b,n,"Chercheuse. Chaque impact ^6aspire^0 les ennemis proches et les écrase. Dégâts ^1%d^0.",d); break;
    case BK_ENCLUME: snprintf(b,n,"Toujours ^1critique^0, assomme et repousse. Dégâts ^1%d^0.",d); break;
    case BK_RAYON: snprintf(b,n,"Traverse tout et projette sans cesse des ^1colonnes de lumière^0 qui aveuglent. Dégâts ^1%d^0.",d); break;
    case BK_KALEIDO: snprintf(b,n,"^6+15%%^0 par rebond, et chaque rebond fait jaillir un ^6éclat^0. Dégâts ^1%d^0.",d); break;
    case BK_GEODE: snprintf(b,n,"Empoisonne, ^4+40%%^0 par altération ; les victimes éclatent en ^6cristaux^0. Dégâts ^1%d^0.",d); break;
    case BK_BLIZZARD: snprintf(b,n,"Bourrasque glaciale : repousse, ralentit et ^4gèle^0 souvent. Dégâts ^1%d^0.",d); break;
    case BK_TEMPFEU: snprintf(b,n,"Bourrasque de flammes : enflamme tout autour et laisse un ^5brasier^0. Dégâts ^1%d^0.",d); break;
    case BK_AURORE: snprintf(b,n,"Traverse ^13^0 ennemis, aveugle, ^3soigne^0 souvent et fait tomber plus d'^4expérience^0. Dégâts ^1%d^0.",d); break;
    case BK_ECHO: snprintf(b,n,"Chaque impact laisse un ^6écho^0 : une onde frappe tout autour 0,4 s plus tard (^6%d^0 dégâts, rayon ^6%d^0). Dégâts ^1%d^0.",4+3*lvl,20+4*lvl,d); break;
    case BK_DOREE: snprintf(b,n,"Plus vous êtes riche, plus elle frappe fort : ^1+1 %%^0 de dégâts par tranche de 20 "IC_COIN" (max ^1+%d %%^0). ^1%d %%^0 de chances de faire tomber une pièce. Dégâts ^1%d^0.",30+15*lvl,6+3*lvl,d); break;
    case BK_RACINE: snprintf(b,n,"^2%d %%^0 de chances d'^2enraciner^0 (%.1f s) : la cible ne bouge plus et subit ^2+25 %%^0 de dégâts. Dégâts ^1%d^0.",15+10*lvl,1.2f+0.3f*lvl,d); break;
    case BK_ACIDE: snprintf(b,n,"Ronge : ^2+%d corrosion^0 par coup (max ^2%d^0). Chaque charge : ^2+5 %%^0 de dégâts subis ; dès 3 charges, armures et carapaces ^2fondent^0. Dégâts ^1%d^0.",lvl>=3?2:1,4+2*lvl,d); break;
    case BK_TONNERRE: snprintf(b,n,"Un éclair à chaque impact, puis un ^6coup de tonnerre^0 (^626^0 dégâts, rayon ^638^0) qui foudroie et étourdit. Dégâts ^1%d^0.",d); break;
    case BK_MIDAS: snprintf(b,n,"^1+1 %%^0 de dégâts par tranche de 10 "IC_COIN" (max ^1+100 %%^0). Les monstres qu'elle achève se changent en ^1or^0. Dégâts ^1%d^0.",d); break;
    case BK_RONCES: snprintf(b,n,"Enracine à coup sûr, fait ^3saigner^0 et fait pousser un ^2roncier^0 qui enracine et lacère les passants. Dégâts ^1%d^0.",d); break;
    case BK_PLUIE: snprintf(b,n,"Éclate en ^2pluie acide^0 : ^2+2 corrosion^0 à tous autour (^212^0 dégâts) et laisse une flaque qui ronge. Dégâts ^1%d^0.",d); break;
    case BK_GHOST: snprintf(b,n,"Âme libérée : traverse tout pendant 4 s. Dégâts ^1%d^0.",d); break;
    case BK_OMBRE: snprintf(b,n,"^6Marque^0 la cible : elle subit ^6+%d %%^0 de dégâts de toutes les sources pendant 4 s. Dégâts ^1%d^0.",15+5*lvl,d); break;
    case BK_RICOCHET: snprintf(b,n,"Chaque rebond sur un mur ajoute ^1+%d %%^0 de dégâts jusqu'à son retour (10 rebonds au plus). Dégâts ^1%d^0.",12+4*lvl,d); break;
    case BK_LAVE: snprintf(b,n,"Chaque impact laisse une ^5flaque de lave^0 qui brûle les monstres pendant ^5%d s^0. Dégâts ^1%d^0.",2+lvl,d); break;
    case BK_BOOMERANG: snprintf(b,n,"Traverse les monstres puis fait ^1demi-tour^0 vers vous : il frappe à l'aller et au retour (^1+%d %%^0 de dégâts au retour). Dégâts ^1%d^0.",20+10*lvl,d); break;
    case BK_SEISME: snprintf(b,n,"Chaque rebond sur un mur déclenche une ^5onde de choc^0 le long du mur (^1%d^0 dégâts, ^5%d %%^0 de chances d'étourdir). Dégâts ^1%d^0.",6+4*lvl,15+5*lvl,d); break;
    case BK_CHAINE: snprintf(b,n,"^4Enchaîne^0 le monstre touché à son voisin pendant 4 s : ^4%d %%^0 des dégâts subis par l'un frappent aussi l'autre. Dégâts ^1%d^0.",35+10*lvl,d); break;
    case BK_CHAKRAM: snprintf(b,n,"Traverse tout, fait ^1demi-tour deux fois^0 et fait ^3saigner^0 tout ce qu'il touche. Dégâts ^1%d^0.",d); break;
    case BK_FAILLE: snprintf(b,n,"Chaque impact et chaque rebond sur un mur ouvre une ^5faille^0 qui traverse toute la largeur du puits et étourdit. Dégâts ^1%d^0.",d); break;
    case BK_ENTRAVE: snprintf(b,n,"Enchaîne la cible à ses ^43 voisins^0 (^460 %%^0 des dégâts partagés), les attire vers elle et les ralentit. Dégâts ^1%d^0.",d); break;
    case BK_APOCALYPSE: snprintf(b,n,"^6Fusion légendaire.^0 Chaque impact déclenche une ^5éruption^0 et une ^5faille^0 sur toute la largeur du puits ; la lave reste au sol. Dégâts ^1%d^0.",d); break;
    case BK_ETOILE: snprintf(b,n,"^4%d %%^0 de chances par impact d'appeler une ^4étoile filante^0 sur un autre monstre (^1%d^0 dégâts autour). Dégâts ^1%d^0.",20+8*lvl,10+6*lvl,d); break;
    case BK_ECLIPSE: snprintf(b,n,"Traverse ^12^0 ennemis, aveugle et marque (^6+40 %%^0 de dégâts subis). Un monstre marqué qui meurt ^6explose en ombre^0. Dégâts ^1%d^0.",d); break;
    case BK_CAUCHEMAR: snprintf(b,n,"Traverse tout. Marque (^6+30 %%^0 de dégâts subis), ralentit et ^6repousse^0 les monstres terrifiés. Dégâts ^1%d^0.",d); break;
    case BK_CARAMBOLE: snprintf(b,n,"Rebonds fous : ^1+18 %%^0 de dégâts par rebond (murs et monstres). Au 6e rebond, elle se ^1dédouble^0. Dégâts ^1%d^0.",d); break;
    case BK_VOLCAN: snprintf(b,n,"Chaque impact provoque une ^5éruption^0 : explosion et grande flaque de lave. Dégâts ^1%d^0.",d); break;
    case BK_CONSTELLATION: snprintf(b,n,"Chaque impact appelle une ^4étoile filante^0 qui se brise en ^43 éclats^0 de cristal. Dégâts ^1%d^0.",d); break;
    case BK_SUPERNOVA: snprintf(b,n,"^535 %%^0 de chances d'appeler une ^5pluie de 3 étoiles^0 brûlantes qui laissent de la lave. Dégâts ^1%d^0.",d); break;
    case BK_BIGBANG: snprintf(b,n,"^6Fusion légendaire.^0 Attire et broie les monstres ; ^625 %%^0 de chances de déclencher un ^6Big Bang^0 (explosion géante et pluie d'étoiles). Dégâts ^1%d^0.",d); break;
    case BK_JUGEMENT: snprintf(b,n,"^6Fusion légendaire.^0 Traverse tout ; chaque impact appelle un ^1rayon divin^0 et un ^4éclair^0 qui frappe 6 monstres. Dégâts ^1%d^0.",d); break;
    case BK_ZERO: snprintf(b,n,"^6Fusion légendaire.^0 ^4Gèle^0 tout ce qu'elle touche ; les monstres gelés qui meurent volent en ^4éclats de glace^0 qui gèlent leurs voisins. Dégâts ^1%d^0.",d); break;
    case BK_OUROBOROS: snprintf(b,n,"^6Fusion légendaire.^0 Fauche les monstres sous ^325 %%^0 de vie, empoisonne et fait saigner ; chaque monstre fauché ^3soigne^0 et propage le poison. Dégâts ^1%d^0.",d); break;
    default: snprintf(b,n,"Dégâts ^1%d^0.",d); break;
  }
}

const PassiveDef PASSIVES[PS_COUNT] = {
  {"Gantelets",SP_ICON_FORCE},{"Sablier",SP_ICON_CADENCE},{"Bottes ailées",SP_ICON_BOTTES},
  {"Cœur de pierre",SP_ICON_COEUR},{"Ressort",SP_ICON_RESSORT},{"Sac de billes",SP_ICON_SAC},
  {"Œil de lynx",SP_ICON_LYNX},{"Lanterne",SP_ICON_LANTERNE},{"Mousse",SP_ICON_MOUSSE},
  {"Filet",SP_ICON_FILET},{"Trèfle",SP_ICON_TREFLE},
  {"Bouclier runique",SP_ICON_BOUCLIER},{"Cœur enragé",SP_ICON_RAGE},{"Fiole d'alchimiste",SP_ICON_FIOLE},{"Miroir",SP_ICON_MIROIR},
  {"Cape d'ombre",SP_ICON_CAPE},{"Loupe",SP_ICON_LOUPE},{"Ancre",SP_ICON_ANCRE},{"Totem d'épines",SP_ICON_TOTEM},
  {"Pendule",SP_ICON_CADENCE},{"Boussole",SP_ICON_LYNX},{"Grimoire",SP_ICON_FIOLE},
};
void passive_desc(int k,int lvl,char *b,int n){
  switch(k){
    case PS_FORCE: snprintf(b,n,"^1+%d%%^0 de dégâts pour toutes les billes.",15*lvl); break;
    case PS_CADENCE: snprintf(b,n,"Lancez ^1%d%%^0 plus vite.",14*lvl); break;
    case PS_BOTTES: snprintf(b,n,"^1+%d%%^0 de vitesse de déplacement.",12*lvl); break;
    case PS_COEUR: snprintf(b,n,"^3+%d PV max^0 et soigne 25 PV.",25*lvl); break;
    case PS_RESSORT: snprintf(b,n,"Billes ^1%d%%^0 plus rapides.",12*lvl); break;
    case PS_SAC: snprintf(b,n,"^1+%d petites billes^0 dans votre réserve.",2*lvl); break;
    case PS_LYNX: snprintf(b,n,"^1+%d%%^0 de coups critiques (dégâts x2).",8*lvl); break;
    case PS_LANTERNE: snprintf(b,n,"Ramassage ^4+%d%%^0 plus loin, ^4+%d%%^0 d'expérience.",40*lvl,10*lvl); break;
    case PS_MOUSSE: snprintf(b,n,"Régénère ^2%.1f PV^0 par seconde.",0.5f*lvl); break;
    case PS_FILET: snprintf(b,n,"^1%d%%^0 de chances qu'une bille rebondisse sur le sol au lieu de revenir.",10+10*lvl); break;
    case PS_TREFLE: snprintf(b,n,lvl>=3?"^1+60%%^0 d'or et un ^1quatrième choix^0 à chaque niveau.":"^1+%d%%^0 d'or ramassé.",20*lvl); break;
    case PS_BOUCLIER: snprintf(b,n,"Une rune ^4bloque un coup^0 toutes les ^4%d s^0.",30-6*lvl); break;
    case PS_RAGE: snprintf(b,n,"^3+%d%%^0 de dégâts par tranche de 10%% de PV manquants.",1+3*lvl); break;
    case PS_FIOLE: snprintf(b,n,"Brûlure, poison et saignement font ^2+%d%%^0 de dégâts et durent plus longtemps.",25*lvl); break;
    case PS_MIROIR: snprintf(b,n,"^6%d%%^0 de chances qu'un lancer crée une ^6copie fantôme^0 de la bille.",4+8*lvl); break;
    case PS_CAPE: snprintf(b,n,"^6%d%%^0 de chances d'^6esquiver^0 un coup (tirs, contacts, gardiens).",8*lvl); break;
    case PS_LOUPE: snprintf(b,n,"^1+%d%%^0 de dégâts contre les élites, les champions et tous les gardiens.",15*lvl); break;
    case PS_ANCRE: snprintf(b,n,"^1+%d%%^0 de dégâts contre les monstres de la moitié basse du puits.",20*lvl); break;
    case PS_TOTEM: snprintf(b,n,"Quand vous êtes blessé, ^3%d épines^0 jaillissent et font saigner.",3+2*lvl); break;
    case PS_PENDULE: snprintf(b,n,"Les monstres descendent ^4%d%%^0 moins vite.",6*lvl); break;
    case PS_BOUSSOLE: snprintf(b,n,"Vos billes s'orientent vers le monstre le plus proche (force ^1%d^0 sur 3).",lvl); break;
    case PS_GRIMOIRE: snprintf(b,n,"^6+%d%%^0 de dégâts pour les évolutions et les fusions légendaires.",15*lvl); break;
    default: snprintf(b,n,"?"); break;
  }
}

/* name, hp, speed, w,h, sprite,sprite2, flags, shoot_cd, xp, coin, breach, contact, blood, child, lore */
const EnemyDef ENEMIES[EK_COUNT] = {
  [EK_SQUELETTE]  = {"Squelette",30,1.0f,12,14,SP_SQUELETTE,-1,0,0,1,0.30f,10,8,HEX(0xeadfc4),-1,"Les anciens gardiens du puits. Ils ne savent plus pourquoi ils descendent."},
  [EK_CHAUVE]     = {"Chauve-souris",14,1.35f,12,8,SP_CHAUVE1,SP_CHAUVE2,EF_ZIGZAG|EF_FLY,0,1,0.2f,8,6,HEX(0xa35fe0),-1,"Volette en zigzag. Fragile mais difficile à viser."},
  [EK_GELEE]      = {"Gelée",26,0.9f,12,10,SP_GELEE,-1,EF_SPLIT,0,1,0.25f,10,8,HEX(0x8be05a),EK_GELETTE,"Se divise en deux gelettes quand elle éclate."},
  [EK_GELETTE]    = {"Gelette",9,1.2f,8,6,SP_GELETTE,-1,0,0,0,0.08f,5,5,HEX(0x8be05a),-1,"Petite, rapide et pressée d'atteindre le fond."},
  [EK_CRANE]      = {"Crâne cracheur",24,0.9f,12,11,SP_CRANE,-1,EF_SHOOT|EF_FLY,3.8f,2,0.35f,10,8,HEX(0xeadfc4),-1,"Crache des éclats d'os droit sur vous."},
  [EK_OSSUAIRE]   = {"Ossuaire",190,0.7f,24,24,SP_OSSUAIRE,-1,EF_ELITE,0,8,1.0f,22,14,HEX(0xeadfc4),-1,"Un amas d'os animé par mille rancunes."},
  [EK_CHAMPIGNON] = {"Champignon",32,0.9f,12,12,SP_CHAMPIGNON,-1,EF_SHOOT,4.4f,2,0.35f,10,8,HEX(0xf59ac0),-1,"Projette des spores en éventail."},
  [EK_ARAIGNEE]   = {"Araignée",18,1.75f,12,9,SP_ARAIGNEE,-1,0,0,1,0.2f,10,8,HEX(0x9a6438),-1,"La plus rapide des créatures de la forêt."},
  [EK_SCARABEE]   = {"Scarabée cornu",46,0.85f,12,11,SP_SCARABEE,-1,EF_PLATE,0,2,0.35f,12,9,HEX(0x45c2a8),-1,"Sa carapace absorbe les coups venant du dessous."},
  [EK_GELEE_TOX]  = {"Gelée toxique",34,0.9f,12,10,SP_GELEE_TOX,-1,EF_SPLIT,0,1,0.25f,10,8,HEX(0xe868c0),EK_GELETTE_TOX,"Une cousine plus coriace de la gelée."},
  [EK_GELETTE_TOX]= {"Gelette toxique",12,1.2f,8,6,SP_GELETTE_TOX,-1,0,0,0,0.08f,5,5,HEX(0xe868c0),-1,"Minuscule et venimeuse."},
  [EK_SPORANGE]   = {"Sporange",240,0.65f,24,24,SP_SPORANGE,-1,EF_ELITE|EF_SHOOT,3.2f,8,1.0f,22,14,HEX(0xf59ac0),-1,"Un champignon géant qui sature l'air de spores."},
  [EK_DIABLOTIN]  = {"Diablotin",30,0.9f,12,12,SP_DIABLOTIN,-1,EF_SHOOT|EF_DRIFT,3.6f,2,0.35f,10,8,HEX(0xff7a22),-1,"Glisse de côté en lançant des boules de feu."},
  [EK_MAGMITE]    = {"Magmite",38,0.85f,12,10,SP_MAGMITE,-1,EF_EMBERS,0,2,0.3f,12,9,HEX(0xff9a2a),-1,"Libère trois braises en mourant."},
  [EK_ARMURE]     = {"Armure animée",52,0.8f,12,14,SP_ARMURE,-1,EF_ARMOR,0,2,0.4f,12,10,HEX(0xc9c4d0),-1,"Blindée : les petits coups ricochent."},
  [EK_CHAUVE_FEU] = {"Chauve-souris de feu",20,1.35f,12,8,SP_CHAUVEF1,SP_CHAUVEF2,EF_ZIGZAG|EF_FLY,0,1,0.2f,8,6,HEX(0xff7a22),-1,"Elle a trop volé près des forges."},
  [EK_GOLEM]      = {"Golem",320,0.6f,24,24,SP_GOLEM,-1,EF_ELITE|EF_ARMOR,0,10,1.0f,25,16,HEX(0xa8a2b4),-1,"Un colosse de pierre au cœur de lave."},
  [EK_OEIL]       = {"Œil rôdeur",34,0.9f,12,12,SP_OEIL,-1,EF_SHOOT|EF_FLY,3.2f,2,0.35f,10,8,HEX(0xc83a5a),-1,"Tire des rafales de trois projectiles."},
  [EK_TENTACULE]  = {"Tentacule",50,0.85f,10,18,-1,-1,EF_REGEN,0,2,0.35f,12,10,HEX(0xa35fe0),-1,"Se régénère dès qu'on la laisse tranquille. Ne relâchez pas la pression."},
  [EK_SPECTRE]    = {"Spectre",30,1.1f,12,12,SP_SPECTRE,-1,EF_PHASE|EF_FLY,0,2,0.3f,10,8,HEX(0xd2b0f5),-1,"Devient intangible par moments."},
  [EK_CHEVALIER]  = {"Chevalier d'ombre",58,0.8f,12,14,SP_CHEVALIER,-1,EF_SHIELD,0,3,0.45f,14,10,HEX(0xa35fe0),-1,"Son bouclier arrête presque tout ce qui vient d'en bas."},
  [EK_OBELISQUE]  = {"Obélisque",48,0.8f,10,16,SP_OBELISQUE,-1,EF_HEAL,0,3,0.45f,12,10,HEX(0xd2b0f5),-1,"Soigne les monstres qui l'entourent."},
  [EK_HORREUR]    = {"Horreur",340,0.55f,24,24,SP_HORREUR,-1,EF_ELITE|EF_SHOOT,2.8f,12,1.0f,28,16,HEX(0xa35fe0),-1,"Mieux vaut ne pas compter ses yeux."},
  [EK_FOLLET]     = {"Feu follet",16,1.15f,10,9,SP_FOLLET1,SP_FOLLET2,EF_FLY|EF_BLINK,0,1,0.25f,8,6,HEX(0x63d6f2),-1,"Disparaît et réapparaît un peu plus loin."},
  [EK_MANTE]      = {"Mante des spores",40,0.85f,12,13,SP_MANTE,-1,EF_DASH,0,2,0.35f,12,9,HEX(0x9be05a),-1,"Guette, puis bondit brusquement vers le fond."},
  [EK_CRACHEUR]   = {"Cracheur de lave",44,0.75f,12,10,SP_CRACHEUR,-1,EF_LOB,4.6f,2,0.4f,12,9,HEX(0xff7a22),-1,"Projette de la lave qui brûle le sol où vous marchez."},
  [EK_SENTVIDE]   = {"Sentinelle du vide",54,0.7f,12,13,SP_SENTVIDE,-1,EF_ORBS|EF_FLY,0,3,0.45f,14,10,HEX(0xc08aff),-1,"Deux orbes tournent autour d'elle et renvoient vos billes."},
  /* ---- les Profondeurs : Grottes de Givre ---- */
  [EK_LOUP]         = {"Loup de givre",26,1.45f,12,11,SP_LOUP,-1,EF_DASH,0,1,0.25f,10,8,HEX(0x9ae8ff),-1,"Il chasse en meute dans les galeries gelées et bondit sans prévenir."},
  [EK_ESPRIT]       = {"Esprit du givre",22,0.95f,10,10,SP_ESPRIT,-1,EF_FLY|EF_FROST,3.6f,2,0.35f,10,8,HEX(0xd4f4ff),-1,"Ses éclats de glace engourdissent : touché, vous ralentissez."},
  [EK_TORTUE]       = {"Tortue de givre",54,0.7f,14,10,SP_TORTUE,-1,EF_PLATE,0,2,0.4f,12,10,HEX(0x63d6f2),-1,"Sa carapace de glace absorbe les coups venant du dessous."},
  [EK_GELEE_GIVRE]  = {"Gelée de givre",34,0.9f,12,10,SP_GELEE_GIVRE,-1,EF_SPLIT,0,1,0.25f,10,8,HEX(0x9ae8ff),EK_GELETTE_GIVRE,"Elle se fend en deux comme un bloc de glace."},
  [EK_GELETTE_GIVRE]= {"Gelette de givre",12,1.25f,8,6,SP_GELETTE_GIVRE,-1,0,0,0,0.08f,5,5,HEX(0x9ae8ff),-1,"Un glaçon pressé de fondre au fond du gouffre."},
  [EK_HARFANG]      = {"Harfang",18,1.35f,12,8,SP_HARFANG1,SP_HARFANG2,EF_FLY|EF_ZIGZAG,0,1,0.2f,8,6,HEX(0xf6f2ea),-1,"Un hibou des neiges qui plonge en silence."},
  [EK_YETI]         = {"Yéti",300,0.6f,24,24,SP_YETI,-1,EF_ELITE|EF_FROST,3.4f,9,1.0f,24,16,HEX(0xd4f4ff),-1,"Il lance des blocs de glace qui engourdissent tout ce qu'ils touchent."},
  /* ---- Cite Engloutie ---- */
  [EK_NOYE]         = {"Noyé",44,0.85f,12,14,SP_NOYE,-1,0,0,1,0.35f,11,9,HEX(0x5af0d0),-1,"Un marin qui n'a jamais cessé de marcher vers le fond."},
  [EK_MURENE]       = {"Murène",24,1.3f,8,13,SP_MURENE,-1,EF_DIVE,0,2,0.3f,10,8,HEX(0xc8d860),-1,"Elle plonge sous l'eau : intouchable, elle file alors vers le fond."},
  [EK_CRABE]        = {"Crabe-forteresse",50,0.75f,14,10,SP_CRABE,-1,EF_PLATE|EF_DRIFT,0,2,0.4f,12,10,HEX(0xe0453a),-1,"Il avance de biais, carapace en avant."},
  [EK_MEDUSE]       = {"Méduse",20,1.0f,10,12,SP_MEDUSE,-1,EF_FLY|EF_ZIGZAG|EF_EMBERS,0,1,0.25f,8,6,HEX(0xe868c0),-1,"Elle éclate en étincelles électriques."},
  [EK_SIRENE]       = {"Sirène",40,0.8f,12,13,SP_SIRENE,-1,EF_HEAL,0,3,0.45f,12,9,HEX(0x5af0d0),-1,"Son chant referme les blessures des monstres alentour."},
  [EK_KRAKEN]       = {"Kraken",380,0.55f,24,24,SP_KRAKEN,-1,EF_ELITE|EF_SUMMON,0,10,1.0f,26,16,HEX(0x9a5ad8),EK_MURENE,"Ses tentacules relâchent sans cesse des murènes."},
  /* ---- Ruche d'Ambre ---- */
  [EK_OUVRIERE]     = {"Ouvrière",16,1.45f,12,9,SP_OUVRIERE1,SP_OUVRIERE2,EF_FLY|EF_ZIGZAG,0,1,0.2f,8,6,HEX(0xffb030),-1,"Rapide, nombreuse, infatigable."},
  [EK_SOLDAT]       = {"Soldat de la ruche",52,0.8f,12,13,SP_SOLDAT,-1,EF_ARMOR,0,2,0.4f,12,10,HEX(0xc99a2e),-1,"Une armure de chitine : les petits coups ricochent."},
  [EK_LARVE]        = {"Larve",40,0.7f,12,8,SP_LARVE,-1,EF_SPLIT,0,1,0.3f,10,8,HEX(0xf6f2ea),EK_LARVULE,"Elle éclate en deux larvules gloutonnes."},
  [EK_LARVULE]      = {"Larvule",13,1.2f,8,5,SP_LARVULE,-1,0,0,0,0.08f,5,5,HEX(0xf6f2ea),-1,"Elle grandira, si vous la laissez faire."},
  [EK_FRELON]       = {"Frelon",26,1.1f,12,10,SP_FRELON,-1,EF_FLY|EF_DASH,0,2,0.3f,10,8,HEX(0xf9dc5c),-1,"Il fait du surplace, puis pique droit vers le fond."},
  [EK_GARDIENNE]    = {"Gardienne d'ambre",46,0.75f,12,12,SP_GARDIENNE,-1,EF_LOB,4.4f,2,0.4f,12,9,HEX(0xffb030),-1,"Elle crache de l'ambre : le sol devient collant et vous ralentit."},
  [EK_COCON]        = {"Cocon",58,0.55f,10,14,SP_COCON,-1,EF_SUMMON,0,3,0.45f,12,10,HEX(0xeadfc4),EK_OUVRIERE,"Des ouvrières en sortent tant qu'il tient debout."},
  [EK_BOURDON]      = {"Bourdon colosse",380,0.55f,24,24,SP_BOURDON,-1,EF_ELITE|EF_SHOOT,2.8f,12,1.0f,28,16,HEX(0xffb030),-1,"Son vrombissement fait trembler les alvéoles."},
  /* ---- Neant ---- */
  [EK_OMBRE]        = {"Ombre",34,1.05f,12,14,SP_OMBRE,-1,EF_PHASE|EF_BLINK,0,2,0.35f,10,8,HEX(0x8a84a0),-1,"Elle disparaît, réapparaît, disparaît encore."},
  [EK_ECHO]         = {"Écho",40,0.85f,12,12,SP_ECHO,-1,EF_SPLIT,0,1,0.3f,10,8,HEX(0xe0f0ff),EK_ECHO_MINEUR,"Chaque coup fatal le divise en échos plus faibles."},
  [EK_ECHO_MINEUR]  = {"Écho mineur",14,1.2f,8,8,SP_ECHO_MINEUR,-1,0,0,0,0.08f,5,5,HEX(0xe0f0ff),-1,"Le souvenir d'un souvenir."},
  [EK_OEIL_NEANT]   = {"Œil du néant",40,0.85f,12,12,SP_OEIL_NEANT,-1,EF_SHOOT|EF_FLY,3.0f,2,0.35f,10,8,HEX(0xe0f0ff),-1,"Il tire des rafales de quatre projectiles."},
  [EK_DEVOREUR]     = {"Dévoreur",66,0.75f,12,12,SP_DEVOREUR,-1,EF_REGEN|EF_ARMOR,0,3,0.45f,13,10,HEX(0x6a5aff),-1,"Blindé, et il se régénère. Frappez fort et vite."},
  [EK_HERAUT]       = {"Héraut du néant",46,0.7f,10,16,SP_HERAUT,-1,EF_FLY|EF_SUMMON,0,3,0.45f,12,10,HEX(0xc08aff),EK_ECHO_MINEUR,"Il appelle les échos depuis l'autre côté."},
  [EK_COLOSSE_NEANT]= {"Colosse du néant",440,0.5f,24,24,SP_COLOSSE_NEANT,-1,EF_ELITE|EF_ARMOR|EF_SHOOT,3.0f,14,1.0f,30,18,HEX(0xe0f0ff),-1,"Une silhouette découpée dans l'absence de toute chose."},
  /* ---- le Royaume oublie : Jardin des Lucioles ---- */
  [EK_LUCIOLE]      = {"Luciole",16,1.4f,10,9,SP_LUCIOLE,-1,EF_FLY|EF_ZIGZAG,0,1,0.2f,8,6,HEX(0xeaff6a),-1,"Elle éclaire le jardin d'une lumière douce. Ne vous y fiez pas."},
  [EK_FLEUR]        = {"Fleur carnivore",38,0.7f,12,13,SP_FLEUR,-1,EF_SHOOT,3.6f,2,0.35f,11,9,HEX(0xff6a9a),-1,"Elle crache ses graines en éventail sur tout ce qui bouge."},
  [EK_ESCARGOT]     = {"Escargot de mousse",58,0.6f,14,9,SP_ESCARGOT,-1,EF_PLATE,0,2,0.4f,12,10,HEX(0x9be05a),-1,"Sa coquille moussue encaisse les coups venus d'en bas."},
  [EK_PAPILLON]     = {"Papillon de nuit",20,1.1f,12,9,SP_PAPILLON1,SP_PAPILLON2,EF_FLY|EF_BLINK,0,1,0.25f,8,6,HEX(0xc8a8ff),-1,"Ses ailes poudrées le font disparaître d'un battement."},
  [EK_SYLVAIN]      = {"Sylvain",360,0.5f,24,24,SP_SYLVAIN,-1,EF_ELITE|EF_HEAL|EF_REGEN,0,10,1.0f,26,16,HEX(0x8ac850),-1,"Un arbre qui marche. Sa sève referme les blessures de tout le jardin."},
  /* ---- Marais maudit ---- */
  [EK_CRAPAUD]      = {"Crapaud des marais",42,0.85f,12,10,SP_CRAPAUD,-1,EF_DASH,0,2,0.35f,12,9,HEX(0x8aa040),-1,"Il guette, gonfle sa gorge, puis bondit vers le fond."},
  [EK_SANGSUE]      = {"Sangsue",30,1.2f,8,12,SP_SANGSUE,-1,EF_DIVE|EF_REGEN,0,2,0.3f,10,8,HEX(0x9a2a4a),-1,"Elle file sous la vase, puis ses plaies se referment."},
  [EK_SORCIERE]     = {"Sorcière des brumes",44,0.75f,12,14,SP_SORCIERE,-1,EF_LOB,4.4f,3,0.45f,12,9,HEX(0x9af05a),-1,"Elle lance des fioles de vase : le sol colle aux bottes."},
  [EK_GELEE_MARAIS] = {"Gelée des marais",38,0.9f,12,10,SP_GELEE_MARAIS,-1,EF_SPLIT,0,1,0.25f,10,8,HEX(0x8a6a3a),EK_GELETTE_MARAIS,"Une gelée gorgée de vase. Elle se divise en deux."},
  [EK_GELETTE_MARAIS]={"Gelette des marais",13,1.2f,8,6,SP_GELETTE_MARAIS,-1,0,0,0,0.08f,5,5,HEX(0x8a6a3a),-1,"Un pâté de vase pressé d'atteindre le fond."},
  [EK_MOUSTIQUE]    = {"Moustique géant",18,1.25f,12,9,SP_MOUSTIQUE1,SP_MOUSTIQUE2,EF_FLY|EF_DASH,0,1,0.2f,8,6,HEX(0xd8203a),-1,"Il fait du surplace, puis pique droit vers le fond."},
  [EK_HYDRE]        = {"Hydre des marais",420,0.5f,24,24,SP_HYDRE,-1,EF_ELITE|EF_SHOOT|EF_REGEN,3.0f,12,1.0f,28,16,HEX(0x6a9a3a),-1,"Trois têtes, trois gueules... et ses plaies se referment."},
  /* ---- Mines de cristal ---- */
  [EK_GOLEM_CRISTAL]= {"Golem de cristal",62,0.7f,12,14,SP_GOLEM_CRISTAL,-1,EF_ARMOR,0,3,0.45f,13,10,HEX(0xc08aff),-1,"Un amas de quartz animé : les petits coups glissent sur ses facettes."},
  [EK_MINEUR]       = {"Mineur fantôme",36,0.9f,12,14,SP_MINEUR,-1,EF_PHASE|EF_SHOOT,3.8f,2,0.4f,11,9,HEX(0x9ad8ff),-1,"Il creuse encore, des siècles après l'effondrement. Gare à sa pioche."},
  [EK_VER]          = {"Ver des roches",40,1.1f,10,14,SP_VER,-1,EF_DIVE,0,2,0.35f,11,9,HEX(0xd89a7a),-1,"Il traverse la roche comme l'eau, invisible et rapide."},
  [EK_ARAIGNEE_CRISTAL]={"Araignée de cristal",22,1.6f,12,9,SP_ARAIGNEE_CRISTAL,-1,EF_EMBERS,0,1,0.25f,10,8,HEX(0xff8ad8),-1,"Elle éclate en éclats coupants quand on la brise."},
  [EK_CHAUVE_CRISTAL]={"Chauve-souris de cristal",20,1.35f,12,8,SP_CHAUVEC1,SP_CHAUVEC2,EF_ZIGZAG|EF_FLY,0,1,0.2f,8,6,HEX(0x63d6f2),-1,"Ses ailes tintent comme du verre."},
  [EK_QUARTZ]       = {"Gardien de quartz",440,0.5f,24,24,SP_QUARTZ,-1,EF_ELITE|EF_ARMOR|EF_ORBS,0,13,1.0f,28,16,HEX(0xff8ad8),-1,"Deux prismes tournent autour de lui et renvoient vos billes."},
  /* ---- Sanctuaire solaire ---- */
  [EK_PRETRE]       = {"Prêtre solaire",46,0.75f,12,14,SP_PRETRE,-1,EF_SHOOT,3.4f,3,0.45f,12,9,HEX(0xffc040),-1,"Il psalmodie et lance des éclats de soleil."},
  [EK_SCARABEE_OR]  = {"Scarabée d'or",56,0.8f,12,11,SP_SCARABEE_OR,-1,EF_PLATE|EF_DRIFT,0,2,0.7f,12,10,HEX(0xffd84a),-1,"Une carapace d'or massif. Il avance de biais et en sème en mourant."},
  [EK_MOMIE]        = {"Momie",70,0.6f,12,14,SP_MOMIE,-1,EF_REGEN,0,2,0.4f,12,10,HEX(0xd8c8a0),-1,"Ses bandelettes se renouent sans cesse. Frappez sans relâche."},
  [EK_PHENIX]       = {"Phénix",28,1.2f,12,9,SP_PHENIX1,SP_PHENIX2,EF_FLY|EF_ZIGZAG|EF_EMBERS,0,2,0.3f,10,8,HEX(0xff8a2a),-1,"Il renaît de ses cendres... en braises qui volent vers vous."},
  [EK_GARDIEN_SOL]  = {"Gardien du soleil",64,0.75f,12,14,SP_GARDIEN_SOL,-1,EF_SHIELD,0,3,0.45f,14,10,HEX(0xffb030),-1,"Son bouclier solaire arrête presque tout ce qui vient d'en bas."},
  [EK_SPHINX]       = {"Sphinx",480,0.45f,24,24,SP_SPHINX,-1,EF_ELITE|EF_SHOOT|EF_SUMMON,3.0f,14,1.0f,30,18,HEX(0xffd84a),EK_SCARABEE_OR,"Il ne pose plus d'énigmes. Il appelle ses scarabées."},
};
void enemy_traits(int k,char *b,int n){
  int f=ENEMIES[k].flags; b[0]=0;
  #define TR(flag,txt) if(f&(flag)){ size_t l=strlen(b); snprintf(b+l,n-(int)l,"%s%s",l?", ":"",txt); }
  TR(EF_ELITE,"Élite") TR(EF_FLY,"Volant") TR(EF_SHOOT,"Tire") TR(EF_SPLIT,"Se divise") TR(EF_ARMOR,"Blindé")
  TR(EF_SHIELD,"Bouclier") TR(EF_PLATE,"Carapace") TR(EF_PHASE,"Intangible") TR(EF_REGEN,"Régénère") TR(EF_HEAL,"Soigneur")
  TR(EF_ZIGZAG,"Zigzague") TR(EF_DRIFT,"Glisse") TR(EF_EMBERS,"Braises") TR(EF_BLINK,"Se téléporte") TR(EF_DASH,"Bondit")
  if(k==EK_GARDIENNE){ TR(EF_LOB,"Ambre") } else if(k==EK_SORCIERE){ TR(EF_LOB,"Vase") } else { TR(EF_LOB,"Lave") } TR(EF_ORBS,"Orbes")
  TR(EF_FROST,"Givre") TR(EF_DIVE,"Plonge") TR(EF_SUMMON,"Invoque")
  #undef TR
  if(!b[0]) snprintf(b,n,"Aucune particularité");
}

const BossDef BOSSES[BOSS_COUNT] = {
  {"Le Roi des Os",1500,64,58,0,"Il régnait sur les catacombes bien avant que le gouffre ne s'ouvre."},
  {"La Mère-Spore",2500,84,52,0,"La forêt entière n'est qu'un seul être : elle."},
  {"Le Colosse de Magma",4000,80,66,3,"Forgé pour garder les forges, il n'a jamais cessé de les garder."},
  {"L'Œil du Gouffre",5000,74,62,0,"Ce qui regarde depuis le fond. Ce que le fond regarde."},
  {"La Reine de Givre",2600,70,64,0,"Elle a figé les grottes pour que rien ne remonte jamais vers la lumière."},
  {"Le Léviathan",4200,96,50,2,"Il dormait sous la cité engloutie depuis qu'elle a sombré. Vous l'avez réveillé."},
  {"La Reine-Mère",6400,92,60,0,"Toute la ruche n'existe que pour la nourrir, et elle a toujours faim."},
  {"Le Cœur du Néant",9500,76,76,3,"Le fond du fond. Ce qui reste quand il ne reste plus rien."},
  /* v7 : le Royaume oublie */
  {"L'Arbre-Lune",3000,84,78,0,"Il a poussé dans le noir, sous une lune qui n'existe pas. Ses lucioles sont ses yeux."},
  {"Le Roi-Crapaud",4600,92,62,1,"Le marais entier lui obéit. Sa langue est plus rapide que votre regard."},
  {"Le Golem-Prisme",6800,78,80,3,"Un cœur de lumière enfermé dans le cristal. Chaque facette est une arme."},
  {"Le Soleil Noir",10000,80,80,2,"Le dieu d'un royaume oublié. Il brille encore, mais sa lumière brûle."},
};

const BiomeDef BIOMES[BIOME_COUNT] = {
  {"Les Catacombes",  HEX(0x3a3442),HEX(0x2e2936),HEX(0x4a4352),HEX(0x5e586a),HEX(0x37323f),HEX(0xeadfc4),HEX(0x5a5a78),
   {EK_SQUELETTE,EK_CHAUVE,EK_GELEE,EK_CRANE,EK_FOLLET},5,EK_OSSUAIRE,BOSS_ROI, 105,1.0f,9.0f,1.0f},
  {"La Forêt Fongique",HEX(0x26362c),HEX(0x1d2b23),HEX(0x314636),HEX(0x40523f),HEX(0x253226),HEX(0xf59ac0),HEX(0x4a6a5a),
   {EK_CHAMPIGNON,EK_ARAIGNEE,EK_SCARABEE,EK_GELEE_TOX,EK_MANTE},5,EK_SPORANGE,BOSS_MERE, 120,1.8f,10.0f,0.94f},
  {"Les Forges Englouties",HEX(0x3c2824),HEX(0x2f1e1b),HEX(0x4d322a),HEX(0x60403a),HEX(0x3c2622),HEX(0xff7a22),HEX(0x7a5048),
   {EK_DIABLOTIN,EK_MAGMITE,EK_ARMURE,EK_CHAUVE_FEU,EK_CRACHEUR},5,EK_GOLEM,BOSS_COLOSSE, 135,3.1f,11.0f,1.06f},
  {"Le Cœur du Gouffre",HEX(0x2b1b38),HEX(0x21152c),HEX(0x3a2648),HEX(0x45305c),HEX(0x26183c),HEX(0xc08aff),HEX(0x5a3a7a),
   {EK_OEIL,EK_TENTACULE,EK_SPECTRE,EK_CHEVALIER,EK_OBELISQUE,EK_SENTVIDE},6,EK_HORREUR,BOSS_OEIL, 140,4.4f,12.0f,0.9f},
  /* ---- les Profondeurs ---- */
  {"Les Grottes de Givre",HEX(0x3e4e62),HEX(0x2e3a4c),HEX(0x52647a),HEX(0x7a8ea6),HEX(0x44566c),HEX(0x9ae8ff),HEX(0x5a6e92),
   {EK_LOUP,EK_ESPRIT,EK_TORTUE,EK_GELEE_GIVRE,EK_HARFANG},5,EK_YETI,BOSS_GIVRE, 120,2.0f,10.0f,0.9f},
  {"La Cité Engloutie",HEX(0x24423e),HEX(0x1a3430),HEX(0x30544e),HEX(0x4a7a72),HEX(0x28463f),HEX(0x5af0d0),HEX(0x3a7a70),
   {EK_NOYE,EK_MURENE,EK_CRABE,EK_MEDUSE,EK_SIRENE},5,EK_KRAKEN,BOSS_LEVIATHAN, 135,3.2f,10.5f,0.85f},
  {"La Ruche d'Ambre",HEX(0x4a3418),HEX(0x382610),HEX(0x5e4420),HEX(0x8a6428),HEX(0x4e3616),HEX(0xffb030),HEX(0x7a5a2a),
   {EK_OUVRIERE,EK_SOLDAT,EK_LARVE,EK_FRELON,EK_GARDIENNE,EK_COCON},6,EK_BOURDON,BOSS_RUCHE, 150,4.6f,11.5f,1.08f},
  {"Le Néant",HEX(0x1c1a24),HEX(0x121018),HEX(0x282432),HEX(0x34304a),HEX(0x1a1824),HEX(0xe0f0ff),HEX(0x6e6e98),
   {EK_OMBRE,EK_ECHO,EK_OEIL_NEANT,EK_DEVOREUR,EK_HERAUT},5,EK_COLOSSE_NEANT,BOSS_NEANT, 160,6.4f,12.5f,0.82f},
  /* ---- le Royaume oublie ---- */
  {"Le Jardin des Lucioles",HEX(0x1e3a34),HEX(0x162c28),HEX(0x2a4a40),HEX(0x3a5a46),HEX(0x1e3830),HEX(0xeaff6a),HEX(0x4a7a6a),
   {EK_LUCIOLE,EK_FLEUR,EK_ESCARGOT,EK_PAPILLON,EK_FOLLET},5,EK_SYLVAIN,BOSS_ARBRE, 125,2.2f,10.0f,0.95f},
  {"Le Marais maudit",HEX(0x2a2a1c),HEX(0x1e1e14),HEX(0x363626),HEX(0x44422e),HEX(0x24241a),HEX(0xb8d040),HEX(0x5a5a3a),
   {EK_CRAPAUD,EK_SANGSUE,EK_SORCIERE,EK_GELEE_MARAIS,EK_MOUSTIQUE},5,EK_HYDRE,BOSS_CRAPAUD, 135,3.4f,10.5f,0.88f},
  {"Les Mines de cristal",HEX(0x24222e),HEX(0x1a1822),HEX(0x302c3c),HEX(0x403a50),HEX(0x221e2c),HEX(0xff8ad8),HEX(0x6a5a8a),
   {EK_GOLEM_CRISTAL,EK_MINEUR,EK_VER,EK_ARAIGNEE_CRISTAL,EK_CHAUVE_CRISTAL},5,EK_QUARTZ,BOSS_PRISME, 145,4.8f,11.0f,1.0f},
  {"Le Sanctuaire solaire",HEX(0x4a3a24),HEX(0x3a2c1a),HEX(0x5a4830),HEX(0x7a6040),HEX(0x40301c),HEX(0xffc040),HEX(0x8a6a3a),
   {EK_PRETRE,EK_SCARABEE_OR,EK_MOMIE,EK_PHENIX,EK_GARDIEN_SOL},5,EK_SPHINX,BOSS_SOLEIL, 160,6.6f,12.5f,1.06f},
};

/* maledictions des grandes profondeurs (cumulatives) */
static const char *CURSE_N[5]={"Gardiens enragés","Disette","Horde","Tireurs d'élite","Abîme"};
static const char *CURSE_D[5]={"Gardiens +25 % de PV, attaques plus fréquentes.","Tous les soins sont réduits de moitié.",
  "20 % de monstres en plus.","Projectiles ennemis 25 % plus rapides.","Élites et champions +40 % de PV, champions plus fréquents."};
const char *curse_name(int d){ return d>=6&&d<=10 ? CURSE_N[d-6] : ""; }
const char *curse_desc(int d){ return d>=6&&d<=10 ? CURSE_D[d-6] : ""; }

const CharDef CHARS[CH_COUNT] = {
  {"La Sentinelle","Gardienne du puits","Rempart",
   "Subit 35% de dégâts en moins quand un ennemi atteint le fond du gouffre.",
   SP_SENTINELLE,BK_FER,100,1.0f,1.0f,1.0f,5,0,HEX(0x3b7fdc),0},
  {"L'Alchimiste","Maîtresse des fioles","Catalyse",
   "Brûlure, poison et saignement infligent 60% de dégâts en plus.",
   SP_ALCHIMISTE,BK_VENIN,85,1.0f,1.0f,1.0f,4,1,HEX(0x3fa34d),0},
  {"Le Jongleur","Virtuose du lancer","Virtuose",
   "4 petites billes de plus et 25% de cadence, mais 15% de dégâts en moins.",
   SP_JONGLEUR,BK_ECLAT,85,1.05f,0.85f,1.25f,9,2,HEX(0xe0453a),0},
  {"La Forgeronne","Bras de fer","Trempe",
   "30% de dégâts en plus et critiques x2,5, mais 10% plus lente.",
   SP_FORGERONNE,BK_PLOMB,130,0.9f,1.3f,1.0f,4,3,HEX(0xdc8a48),0},
  {"Le Magnétiseur","Maître des champs","Champ magnétique",
   "Les billes qui retombent près de lui repartent vers le haut. Attire les gemmes de très loin.",
   SP_MAGNETISEUR,BK_AIMANT,90,1.0f,1.0f,1.0f,5,4,HEX(0xb87333),0},
  {"La Nécromancienne","Maîtresse des âmes","Moisson d'âmes",
   "Chaque monstre vaincu a 12% de chances de libérer une âme : une bille spectrale qui traverse tout pendant 4 s.",
   SP_NECRO,BK_SPECTRE,80,1.0f,1.0f,1.0f,4,0,HEX(0x7a3ab8),1},
  {"L'Horlogère","Gardienne du temps","Arrêt du temps",
   "Toutes les 14 s, le temps se fige à moitié : monstres et projectiles ralentissent fortement pendant 3 s.",
   SP_HORLOGERE,BK_GLACE,90,1.05f,1.0f,1.0f,5,0,HEX(0x45c2a8),1},
  {"Le Pyromane","Enfant des braises","Tout brûle",
   "Tous vos coups enflamment. Les monstres en feu explosent en mourant.",
   SP_PYROMANE,BK_FEU,90,1.0f,0.9f,1.0f,5,0,HEX(0xff7a22),1},
  /* v7 */
  {"La Druidesse","Gardienne des racines","Sève",
   "Régénère 0,6 PV par seconde. Chaque coup a 20% de chances d'enraciner sa cible (1,5 s).",
   SP_DRUIDE,BK_RACINE,95,1.0f,1.0f,1.0f,5,5,HEX(0x5aa040),0},
  {"Le Marchand","Négociant des abysses","Bonne affaire",
   "+50% d'or ramassé, et +1% de dégâts par tranche de 25 or en poche (max +40%).",
   SP_MARCHAND,BK_DOREE,90,1.05f,0.95f,1.0f,5,0,HEX(0xe8c040),1},
  /* v9 */
  {"L'Astronome","Lectrice d'étoiles","Pluie d'étoiles",
   "Toutes les 7 s, une étoile filante frappe le monstre le plus résistant. Les étoiles filantes font 25% de dégâts en plus.",
   SP_ASTRONOME,BK_ETOILE,85,1.0f,1.0f,1.0f,5,0,HEX(0x5a7ae0),1},
  {"La Voleuse d'ombres","Lame du crépuscule","Coup de l'ombre",
   "+8% de coups critiques. Chaque coup critique marque sa cible : +25% de dégâts subis pendant 4 s.",
   SP_VOLEUSE,BK_OMBRE,85,1.08f,1.0f,1.0f,5,0,HEX(0x8a4ad8),1},
  /* v10 */
  {"La Chasseresse","Lame qui revient","Retour gagnant",
   "Chaque bille rattrapée donne +2% de dégâts pendant 6 s (jusqu'à +20%).",
   SP_CHASSERESSE,BK_BOOMERANG,90,1.06f,1.0f,1.05f,5,0,HEX(0x4a9a5a),1},
  {"Le Géomancien","Celui qui fait trembler","Secousse",
   "Toutes les 10 s, la terre tremble : tous les monstres subissent des dégâts et sont étourdis.",
   SP_GEOMANCIEN,BK_SEISME,105,0.95f,1.0f,0.95f,5,0,HEX(0xa8763a),1},
};
const char *const CHAMP_NAMES[CHAMP_COUNT]={"","Rapide","Colosse","Régénérant","Bouclier","Explosif","Invocateur","Soigneur","Spectral"};
/* v10 : mutateurs (modificateurs choisis avant un niveau) */
bool mut_unlocked(void){ int i=ach_for_reward(RW_MODE,MUT_UNLOCK); return i>=0 && S.ach[i]; }
int mut_count(uint32_t m){ int n=0; for(int i=0;i<MOD_COUNT;i++) n+=(m>>i)&1; return n; }
int mut_bonus(uint32_t m){ static const int B[MOD_COUNT]={15,10,10,15,10,15,10,10,15,20,15,20}; int t=0; for(int i=0;i<MOD_COUNT;i++) if((m>>i)&1) t+=B[i]; return t; }

const BuildingDef BUILDINGS[BLD_COUNT] = {
  {"Forge","Trempe de meilleures billes : plus de dégâts.",15,40, 150,168},
  {"Infirmerie","Des soins avant chaque descente : plus de PV.",15,35, 244,132},
  {"Atelier","Des mécanismes de lancer plus rapides.",12,45, 396,132},
  {"Bibliothèque","Apprendre plus vite des combats : plus d'XP.",12,40, 490,168},
  {"Trésorerie","Rapporter plus d'or du gouffre.",12,50, 140,262},
  {"Puits aux souhaits","Relancer les choix de montée de niveau.",7,80, 250,286},
  {"Sanctuaire","Revenir d'entre les morts pendant la descente.",3,240, 390,286},
  {"Tour de guet","Se déplacer plus vite dans le gouffre.",8,60, 500,262},
  {"Laboratoire","Des expériences pour partir mieux équipé.",4,150, 576,214},
  {"Autel des héros","Les héros progressent plus vite dans leurs rangs.",5,120, 64,214},
  {"Armurerie","Des pointes et des lames : plus de coups critiques.",10,70, 590,286},
  /* v7 */
  {"Serre","Des herbes qui soignent : régénération pendant la descente.",8,60, 58,150},
  {"Observatoire","Étudier les gardiens : plus de dégâts contre eux.",8,90, 590,152},
  /* v9 */
  {"Creuset légendaire","Fondre deux évolutions en une bille légendaire.",8,140, 320,121},
  {"Cabinet des curiosités","Plus de curiosités, de plus riches trésors, de plus forts pièges.",8,80, 44,292},
};
int building_cost(int b,int lvl){
  if(lvl<1) lvl=1;
  float c=BUILDINGS[b].base_cost*powf((float)lvl,1.5f);
  if(lvl>8) c*=powf(1.9f,(float)(lvl-8));       /* les derniers niveaux coutent beaucoup plus cher */
  return ((int)(c+2.5f)/5)*5;
}
int building_crystals(int b,int lvl){
  if(b==BLD_SANCTUAIRE && lvl>=3) return 12;
  if(b==BLD_LABO && lvl>=4) return 10;
  if(b==BLD_AUTEL && lvl>=3) return 3*(lvl-2);
  if(b==BLD_OBSERV && lvl>=6) return 2*(lvl-5);
  if(b==BLD_CREUSET) return lvl==1 ? 10 : 2*lvl;
  return lvl>8 ? 3*(lvl-8) : 0;
}
void building_effect(int b,int lvl,char *buf,int n){
  switch(b){
    case BLD_FORGE: snprintf(buf,n,"+%d%% de dégâts",6*lvl); break;
    case BLD_INFIRMERIE: snprintf(buf,n,"+%d PV max",12*lvl); break;
    case BLD_ATELIER: snprintf(buf,n,"+%d%% de cadence",5*lvl); break;
    case BLD_BIBLIO: snprintf(buf,n,"+%d%% d'expérience",8*lvl); break;
    case BLD_TRESOR: snprintf(buf,n,"+%d%% d'or",6*lvl); break;
    case BLD_PUITS: snprintf(buf,n,"%d relance%s par descente",lvl,lvl>1?"s":""); break;
    case BLD_SANCTUAIRE: snprintf(buf,n,lvl>=3?"2 résurrections à 100%% PV":(lvl>=2?"Résurrection à 100%% PV":"Résurrection à 50%% PV")); break;
    case BLD_TOUR: snprintf(buf,n,"+%d%% de vitesse",5*lvl); break;
    case BLD_LABO: snprintf(buf,n,lvl>=4?"Bille niv. 3 + un objet niv. 2":(lvl>=3?"Bille niv. 2 + un objet au départ":(lvl>=2?"Bille aléatoire niv. 2 au départ":"Une bille aléatoire au départ"))); break;
    case BLD_AUTEL: snprintf(buf,n,"+%d%% d'XP de héros",10*lvl); break;
    case BLD_ARMURERIE: snprintf(buf,n,"+%d%% de coups critiques",2*lvl); break;
    case BLD_SERRE: snprintf(buf,n,"+%d,%d PV par seconde",lvl/10,lvl%10); break;
    case BLD_OBSERV: snprintf(buf,n,"+%d%% de dégâts contre les gardiens",5*lvl); break;
    case BLD_CREUSET: if(lvl<=1) snprintf(buf,n,"Fusions légendaires débloquées"); else snprintf(buf,n,"Fusions légendaires, +%d%% de dégâts",10*(lvl-1)); break;
    case BLD_CABINET: snprintf(buf,n,"+%d%% de curiosités, trésors et pièges",12*lvl); break;
    default: snprintf(buf,n,"?"); break;
  }
  if(lvl<=0) snprintf(buf,n,"Aucun effet");
}

/* ---------------- v9 : objectifs de niveau ---------------- */
typedef struct { int type, n; } LvlObj;
static const LvlObj OBJ[BIOME_COUNT][3]={
  {{OB_CURIO,6},{OB_BOSS,120},{OB_BREACH,5}}, {{OB_CURIO,6},{OB_COMBO,150},{OB_BREACH,5}}, {{OB_CURIO,7},{OB_BOSS,120},{OB_CHAMP,3}}, {{OB_CURIO,7},{OB_LEVEL,25},{OB_BREACH,4}},
  {{OB_CURIO,7},{OB_BOSS,110},{OB_COMBO,200}}, {{OB_CURIO,8},{OB_CHAMP,4},{OB_BREACH,4}}, {{OB_CURIO,8},{OB_BOSS,110},{OB_LEVEL,28}}, {{OB_CURIO,8},{OB_COMBO,250},{OB_BREACH,3}},
  {{OB_CURIO,8},{OB_BOSS,100},{OB_CHAMP,5}}, {{OB_CURIO,9},{OB_LEVEL,30},{OB_BREACH,3}}, {{OB_CURIO,9},{OB_BOSS,100},{OB_COMBO,300}}, {{OB_CURIO,10},{OB_CHAMP,6},{OB_BREACH,2}},
};
int obj_kind(int L,int k){ return OBJ[clampi(L,0,BIOME_COUNT-1)][clampi(k,0,2)].type; }
void obj_text(int L,int k,char *b,int n){
  const LvlObj *o=&OBJ[clampi(L,0,BIOME_COUNT-1)][clampi(k,0,2)];
  switch(o->type){
    case OB_CURIO: snprintf(b,n,"Briser %d curiosités",o->n); break;
    case OB_BOSS: snprintf(b,n,"Vaincre le gardien en moins de %d:%02d",o->n/60,o->n%60); break;
    case OB_BREACH: snprintf(b,n,o->n>1?"Laisser passer %d monstres au plus":"Laisser passer un monstre au plus",o->n); break;
    case OB_CHAMP: snprintf(b,n,"Vaincre %d champions",o->n); break;
    case OB_COMBO: snprintf(b,n,"Atteindre un combo de %d",o->n); break;
    default: snprintf(b,n,"Atteindre le niveau %d",o->n); break;
  }
}
int obj_value(int L,int k,int *target){
  const LvlObj *o=&OBJ[clampi(L,0,BIOME_COUNT-1)][clampi(k,0,2)]; *target=o->n;
  switch(o->type){
    case OB_CURIO: return R.curios_run;
    case OB_BOSS: return (int)R.boss_t;
    case OB_BREACH: return R.breaches;
    case OB_CHAMP: return R.champ_kills;
    case OB_COMBO: return R.combo_max;
    default: return R.plevel;
  }
}
bool obj_met(int L,int k){
  int t; int v=obj_value(L,k,&t); int ty=OBJ[clampi(L,0,BIOME_COUNT-1)][clampi(k,0,2)].type;
  if(ty==OB_BOSS) return R.won && v<t;
  if(ty==OB_BREACH) return v<=t;
  return v>=t;
}
int obj_total(void){ int n=0; for(int L=0;L<BIOME_COUNT;L++) for(int k=0;k<3;k++) n+=(S.obj_done[L]>>k)&1; return n; }

const DiffDef DIFFS[DIFF_COUNT] = {
  {"Facile","Monstres et gardiens ^2-30 %^0 de PV, dégâts subis ^2-40 %^0, monstres plus lents, ^2soin complet^0 après chaque gardien. Or et XP de héros ^3-20 %^0.",
   "Monstres ^2-30 %^0 de PV, dégâts subis ^2-40 %^0, soin complet après chaque gardien ; or et XP ^3-20 %^0",
   0.70f,0.60f,0.70f,0.90f,0.80f,1.00f,HEX(0x8be05a)},
  {"Normale","Le gouffre tel qu'il a été pensé : exigeant mais juste. Chaque gardien vaincu rend ^250 %^0 des PV.",
   "Le gouffre tel qu'il a été pensé : exigeant mais juste. Chaque gardien vaincu rend ^250 %^0 des PV.",
   1.0f,1.0f,1.0f,1.0f,1.0f,0.50f,HEX(0xf0e6d0)},
  {"Difficile","Monstres et gardiens ^3+30 %^0 de PV, dégâts subis ^3+30 %^0, monstres plus rapides, soins réduits après les gardiens. Or et XP de héros ^1+25 %^0.",
   "Monstres ^3+30 %^0 de PV, dégâts subis ^3+30 %^0, soins réduits ; or et XP de héros ^1+25 %^0",
   1.30f,1.30f,1.30f,1.06f,1.25f,0.30f,HEX(0xff5a4a)},
};
const DiffDef *diff_cur(void){ return &DIFFS[clampi(S.difficulty,0,DIFF_COUNT-1)]; }

const ModDef MODS[MOD_COUNT] = {
  {"Canon de verre","+50% de dégâts, mais -40% de PV max."},
  {"Ruée","Monstres 25% plus rapides, +40% d'expérience."},
  {"Fièvre de l'or","+100% d'or, mais monstres +25% de PV."},
  {"Carapaces","Tous les monstres encaissent mieux les petits coups."},
  {"Jonglerie","+6 petites billes, mais -20% de dégâts."},
  {"Gardiens enragés","Les gardiens arrivent enragés. +1 cristal par gardien."},
  {"Brouillard","Le gouffre est plus sombre. +30% d'expérience."},
  {"Champions","Beaucoup plus de champions. +50% d'or."},
  {"Horde","35% de monstres en plus. +25% d'expérience."},
  {"Sans soin","Plus aucun soin (cœurs, régénération, talents). +30% d'or."},
  {"Fragile","Les monstres qui passent font deux fois plus mal. +30% d'expérience."},
  {"Élite","Élites, champions et gardiens mineurs +50% de PV, plus de champions. +40% d'or."},
};

/*!\file window.c
 *
 * \brief Génération de paysage fractal à l'aide de l'algorithme de
 * déplacement des milieux. Un algorithme de midpoint displacement est
 * proposé dans GL4Dummies : le Triangle-Edge.
 *
 * \author Farès BELHADJ, amsi@ai.univ-paris8.fr
 * \date March 07 2017
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <GL4D/gl4du.h>
#include <GL4D/gl4dg.h>
#include <GL4D/gl4duw_SDL2.h>
#include <SDL_image.h>

/*fonctions externes dans assimp.c */
extern void assimpInit(const char * filename);
extern void assimpInit2(const char * filename);
extern void assimpDrawScene(void);
extern void assimpQuit(void);

/* fonctions externes dans noise.c */
extern void initNoiseTextures(void);
extern void useNoiseTextures(GLuint pid, int shift);
extern void unuseNoiseTextures(int shift);
extern void freeNoiseTextures(void);
/* fonctions locales, statiques */
static void quit(void);
static void init(void);
static void resize(int w, int h);
static void idle(void);
static void keydown(int keycode);
static void keyup(int keycode);
static void draw(void);
static GLfloat heightMapAltitude(GLfloat x, GLfloat z);

/*!\brief largeur de la fenêtre */
static int _windowWidth = 800;
/*!\brief haiteur de la fenêtre */
static int _windowHeight = 600;
/*!\brief largeur de la heightMap générée */
static int _landscape_w = 550;
/*!\brief heuteur de la heightMap générée */
static int _landscape_h = 550;
/*!\brief scale en x et z du modèle de terrrain */
static GLfloat _landscape_scale_xz = 150.0f;
/*!\brief scale en y du modèle de terrain */
static GLfloat _landscape_scale_y = 10.0f;
/*!\brief heightMap du terrain généré */
static GLfloat * _heightMap = NULL;
static GLfloat * _heightMap2 = NULL;
/*!\brief identifiant d'un plan (eau) */
static GLuint _plan = 0;
/*!\brief identifiant du terrain généré */
static GLuint _landscape = 0;
static GLuint _landscape2 = 0;
/*!\brief identifiant GLSL program du terrain */
static GLuint _landscape_pId  = 0;
static GLuint _landscape_pId2  = 0;
static GLuint _yellowred_pId=0;
static GLuint tree_pId  = 0;
/*!\brief identifiant de la texture de dégradé de couleurs du terrain */
static GLuint _terrain_tId = 0;
static GLuint _terrain_tId2 = 0;
static GLuint _plasma_tId = 0;
static GLuint _yellowred_tId=0;
static GLuint tree_tId=0;
/*!\brief déphasage du cycle */
static GLfloat _cycle = 0.0f;

GLfloat n=0.5f;
GLfloat j=0.4f;
int cmp=0,tour, verif=0;
/*!\brief indices des touches de clavier */


enum kyes_t {
  KLEFT = 0,
  KRIGHT,
  KUP,
  KDOWN
};

/*!\brief clavier virtuel */
static GLuint _keys[] = {0, 0, 0, 0};

typedef struct cam_t cam_t;
/*!\brief structure de données pour la caméra */
struct cam_t {
  GLfloat x, z;
  GLfloat theta;
};

/*!\brief la caméra */
static cam_t _cam = {0, 0, 0};
static GLuint _sphere = 0;

/*!\brief création de la fenêtre, paramétrage et initialisation,
 * lancement de la boucle principale */
int main(int argc, char ** argv) {
  if(!gl4duwCreateWindow(argc, argv, "Landscape", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                         _windowWidth, _windowHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN))
    return 1;
  init();
  atexit(quit);
  gl4duwResizeFunc(resize);
  assimpInit("./tree/Tree2.3ds");
  gl4duwKeyUpFunc(keyup);
  gl4duwKeyDownFunc(keydown);
  gl4duwDisplayFunc(draw);
  gl4duwIdleFunc(idle);
  gl4duwMainLoop();
  return 0;
}

/*!\brief paramétrage OpenGL et initialisation des données */
static void init(void) {
  SDL_Surface * t;
   /* generates a sphere using GL4Dummies */
  _sphere = gl4dgGenSpheref(10, 10);

  /* pour générer une chaine aléatoire différente par exécution */
  srand(time(NULL));
  /* paramètres GL */
  glClearColor(0.0f, 0.4f, 0.9f, 0.0f);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  /* chargement et compilation des shaders */
  _landscape_pId  = gl4duCreateProgram("<vs>shaders/basic.vs", "<fs>shaders/basic.fs", NULL);
   _landscape_pId2  = gl4duCreateProgram("<vs>shaders/basic.vs", "<fs>shaders/basic.fs", NULL); 
  tree_pId = gl4duCreateProgram("<vs>shaders/basic2.vs", "<fs>shaders/basic2.fs", NULL); 
   _yellowred_pId= gl4duCreateProgram("<vs>shaders/basic3.vs", "<fs>shaders/sun.fs", NULL);
  /* création des matrices de model-view et projection */
  gl4duGenMatrix(GL_FLOAT, "modelViewMatrix");
  gl4duGenMatrix(GL_FLOAT, "projectionMatrix");
  /* appel forcé à resize pour initialiser le viewport et les matrices */
  resize(_windowWidth, _windowHeight);
  /* création de la géométrie du plan */
  _plan = gl4dgGenQuadf();
  /* génération de la heightMap */
  _heightMap = gl4dmTriangleEdge(_landscape_w, _landscape_h, 0.4);
  
  /* création de la géométrie du terrain en fonction de la heightMap */
  _landscape = gl4dgGenGrid2dFromHeightMapf(_landscape_w, _landscape_h, _heightMap);
   _heightMap2 = gl4dmTriangleEdge(_landscape_w, _landscape_h, 0.2);
  _landscape2 =gl4dgGenGrid2dFromHeightMapf(_landscape_w, _landscape_h, _heightMap2);
  /* création, paramétrage, chargement et transfert de la texture
     contenant le dégradé de couleurs selon l'altitude (texture 1D) */
  glGenTextures(1, &_terrain_tId);
  glBindTexture(GL_TEXTURE_1D, _terrain_tId);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  t = IMG_Load("desert.jpg");
  assert(t);
 #ifdef __APPLE__
  glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, t->w, 0, t->format->BytesPerPixel == 3 ? GL_BGR : GL_BGRA, GL_UNSIGNED_BYTE, t->pixels);
#else
  glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, t->w, 0, t->format->BytesPerPixel == 3 ? GL_RGB : GL_RGBA, GL_UNSIGNED_BYTE, t->pixels);
#endif

  SDL_FreeSurface(t);

  glGenTextures(1, &_terrain_tId2);
  glBindTexture(GL_TEXTURE_1D, _terrain_tId2);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  t = IMG_Load("alt.png");
  assert(t);
#ifdef __APPLE__
  glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, t->w, 0, t->format->BytesPerPixel == 3 ? GL_BGR : GL_BGRA, GL_UNSIGNED_BYTE, t->pixels);
#else
  glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, t->w, 0, t->format->BytesPerPixel == 3 ? GL_RGB : GL_RGBA, GL_UNSIGNED_BYTE, t->pixels);
#endif
  SDL_FreeSurface(t);
  initNoiseTextures();




  glGenTextures(1, &tree_tId);
  glBindTexture(GL_TEXTURE_2D, tree_tId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   t = IMG_Load("./tree/Branches0018_1_S.png");
  assert(t);
#ifdef __APPLE__
  glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, t->w, 0, t->format->BytesPerPixel == 3 ? GL_BGR : GL_BGRA, GL_UNSIGNED_BYTE, t->pixels);
#else
  glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, t->w, 0, t->format->BytesPerPixel == 3 ? GL_RGB : GL_RGBA, GL_UNSIGNED_BYTE, t->pixels);
#endif
  SDL_FreeSurface(t);


}

/*!\brief paramétrage du viewport OpenGL et de la matrice de
 * projection en fonction de la nouvelle largeur et heuteur de la
 * fenêtre */
static void resize(int w, int h) {
  glViewport(0, 0, _windowWidth = w, _windowHeight = h);
  gl4duBindMatrix("projectionMatrix");
  gl4duLoadIdentityf();
  gl4duFrustumf(-0.5, 0.5, -0.5 * _windowHeight / _windowWidth, 0.5 * _windowHeight / _windowWidth, 1.0, 1000.0);
}

/*!\brief récupération du delta temps entre deux appels */
static double get_dt(void) {
  static double t0 = 0, t, dt;
  t = gl4dGetElapsedTime();
  dt = (t - t0) / 1000.0;
  t0 = t;
  return dt;
}



/*!\brief simulation : modification des paramètres de la caméra
 * (LookAt) en fonction de l'interaction clavier */
static void idle(void) {  // je me suis inspirée de la fonction de Hatim 
  double dt, dtheta = M_PI, pas = 50.0;
  dt = get_dt();
  if((cmp>=10)&&(cmp<=1000)){
    _cycle += dt;
    tour++;
    cmp=0;
  if((tour>=45)&&(verif!=1)){
      n=n-0.03;      
      j=j-0.03;
      if((n>=0.12)&&(j>=0.02)){
        glClearColor(0.0f, j, n, 0.0f);
      }
  }
  if(tour>100){
    verif=1;
    n=j+0.02;      
    j=j+0.02;
      if((n<=0.5)&&(j<=0.4)){
            glClearColor(0.0f, j, n, 0.0f);
      }
     glClearColor(0.0f, 0.4f, 0.9f, 0.0f);
  }
  if(tour==150){
    tour=0;
    verif=0;
  }
  }

  if(_keys[KLEFT]) {
    _cam.theta += dt * dtheta;
  }
  if(_keys[KRIGHT]) {
    _cam.theta -= dt * dtheta;
  }
  if(_keys[KUP]) {
    _cam.x += -dt * pas * sin(_cam.theta);
    _cam.z += -dt * pas * cos(_cam.theta);
    cmp++;
  }
  if(_keys[KDOWN]) {
    _cam.x += dt * pas * sin(_cam.theta);
    _cam.z += dt * pas * cos(_cam.theta);
    cmp++;
  }
}



/*!\brief interception et gestion des événements "down" clavier */
static void keydown(int keycode) {
  GLint v[2];
  switch(keycode) {
  case SDLK_LEFT:
    _keys[KLEFT] = 1;
    break;
  case SDLK_RIGHT:
    _keys[KRIGHT] = 1;
    break;
  case SDLK_UP:
    _keys[KUP] = 1;
    break;
  case SDLK_DOWN:
    _keys[KDOWN] = 1;
    break;
  case 'w':
    glGetIntegerv(GL_POLYGON_MODE, v);
    if(v[0] == GL_FILL)
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    break;
  case SDLK_ESCAPE:
  case 'q':
    exit(0);
  default:
    break;
  }
}

/*!\brief interception et gestion des événements "up" clavier */
static void keyup(int keycode) {
  switch(keycode) {
  case SDLK_LEFT:
    _keys[KLEFT] = 0;
    break;
  case SDLK_RIGHT:
    _keys[KRIGHT] = 0;
    break;
  case SDLK_UP:
    _keys[KUP] = 0;
    break;
  case SDLK_DOWN:
    _keys[KDOWN] = 0;
    break;
  }
}


/*!\brief dessin de la frame */
static void draw(void) {
  GLfloat cam[2] = {0, 0};
  /* coordonnées de la souris */
  int xm, ym;
  SDL_PumpEvents();
  SDL_GetMouseState(&xm, &ym);
  /* position de la lumière (temp et lumpos), altitude de la caméra et matrice courante */
  GLfloat temp[4] = {100, 100, 0, 1.0}, lumpos[4], landscape_y, *mat;
  landscape_y = heightMapAltitude(_cam.x, _cam.z);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  gl4duBindMatrix("modelViewMatrix");
  gl4duLoadIdentityf();
  gl4duLookAtf(_cam.x, landscape_y + 2.0, _cam.z, 
	       _cam.x - sin(_cam.theta), landscape_y + 2.0 - (ym - (_windowHeight >> 1)) / (GLfloat)_windowHeight, _cam.z - cos(_cam.theta), 
	       0.0, 1.0,0.0);


  /* utilisation du shader de terrain */
  glUseProgram(_landscape_pId);
  mat = gl4duGetMatrixData();
  MMAT4XVEC4(lumpos, mat, temp);
  gl4duScalef(_landscape_scale_xz, _landscape_scale_y, _landscape_scale_xz);
  gl4duSendMatrices();
  glUniform4fv(glGetUniformLocation(_landscape_pId, "lumpos"), 1, lumpos);
  glUniform1i(glGetUniformLocation(_landscape_pId, "degrade"), 0);
  glUniform1i(glGetUniformLocation(_landscape_pId, "eau"), 0);
  glUniform1f(glGetUniformLocation(_landscape_pId, "cycle"), _cycle);
  glBindTexture(GL_TEXTURE_1D, _terrain_tId);
  useNoiseTextures(_landscape_pId, 1);

   
    /* tells pId that the sky is false */
  glUniform1i(glGetUniformLocation(_landscape_pId, "sky"), 1);
  cam[0] = _cam.x / (2.0 * 1000);
  cam[1] = -_cam.z / (2.0 * 1000);
  glUniform2fv(glGetUniformLocation(_landscape_pId, "cam"), 1, cam);
  gl4dgDraw(_landscape);
  gl4duRotatef(-90, 1, 0, 0);
  gl4duSendMatrices();
  glUniform1i(glGetUniformLocation(_landscape_pId, "eau"), 1);
  gl4dgDraw(_plan);
  unuseNoiseTextures(1);


 /* shader de l'ile */
  glUseProgram(_landscape_pId2);
  gl4duRotatef(90, -1, 0, 0);
  MMAT4XVEC4(lumpos, mat, temp);
  gl4duTranslatef(0,0,5);
  gl4duSendMatrices();
  glUniform4fv(glGetUniformLocation(_landscape_pId2, "lumpos"), 1, lumpos);
  glUniform1i(glGetUniformLocation(_landscape_pId2, "cam"), 1);
  glBindTexture(GL_TEXTURE_2D, _terrain_tId2);
  gl4dgDraw(_landscape2);
 
  /* Tree */
  glUseProgram(tree_pId);
  glUniform4fv(glGetUniformLocation(tree_pId, "lumpos"), 1, lumpos);
  gl4duLoadIdentityf();
  gl4duLookAtf(_cam.x, landscape_y + 2.0, _cam.z, 
	       _cam.x - sin(_cam.theta), landscape_y + 2.0 - (ym - (_windowHeight >> 1)) / (GLfloat)_windowHeight, _cam.z - cos(_cam.theta), 
	       0.0, 1.0,0.0);
  float y = heightMapAltitude(1, -3);
  gl4duTranslatef(1, y + 3,-3);
  gl4duScalef(5, 5, 5);
  glBindTexture(GL_TEXTURE_2D, tree_tId);
  assimpDrawScene();

  glUniform4fv(glGetUniformLocation(tree_pId, "lumpos"), 1, lumpos);
  gl4duLoadIdentityf();
  gl4duLookAtf(_cam.x, landscape_y + 2.0, _cam.z, 
	       _cam.x - sin(_cam.theta), landscape_y + 2.0 - (ym - (_windowHeight >> 1)) / (GLfloat)_windowHeight, _cam.z - cos(_cam.theta), 
	       0.0, 1.0,0.0);
   y = heightMapAltitude(8, -5);
  gl4duTranslatef(4, y + 3,-5);
  gl4duScalef(5, 5, 5);
  glBindTexture(GL_TEXTURE_2D, tree_tId);
  assimpDrawScene();

  glUniform4fv(glGetUniformLocation(tree_pId, "lumpos"), 1, lumpos);
  gl4duLoadIdentityf();
  gl4duLookAtf(_cam.x, landscape_y + 2.0, _cam.z, 
	       _cam.x - sin(_cam.theta), landscape_y + 2.0 - (ym - (_windowHeight >> 1)) / (GLfloat)_windowHeight, _cam.z - cos(_cam.theta), 
	       0.0, 1.0,0.0);
   y = heightMapAltitude(-3, -3);
  gl4duTranslatef(-3, y + 3,-3);
  gl4duScalef(5, 5, 5);
  glBindTexture(GL_TEXTURE_2D, tree_tId);
  assimpDrawScene();

   glUniform4fv(glGetUniformLocation(tree_pId, "lumpos"), 1, lumpos);
  gl4duLoadIdentityf();
  gl4duLookAtf(_cam.x, landscape_y + 2.0, _cam.z, 
	       _cam.x - sin(_cam.theta), landscape_y + 2.0 - (ym - (_windowHeight >> 1)) / (GLfloat)_windowHeight, _cam.z - cos(_cam.theta), 
	       0.0, 1.0,0.0);
  y = heightMapAltitude(-1, -3);
  gl4duTranslatef(-1, y + 3,-3);
  gl4duScalef(5, 5, 5);
  glBindTexture(GL_TEXTURE_2D, tree_tId);
  assimpDrawScene();

  glUniform4fv(glGetUniformLocation(tree_pId, "lumpos"), 1, lumpos);
  gl4duLoadIdentityf();
  gl4duLookAtf(_cam.x, landscape_y + 2.0, _cam.z, 
	       _cam.x - sin(_cam.theta), landscape_y + 2.0 - (ym - (_windowHeight >> 1)) / (GLfloat)_windowHeight, _cam.z - cos(_cam.theta), 
	       0.0, 1.0,0.0);
   y = heightMapAltitude(-10, -3);
  gl4duTranslatef(-10, y+3 ,-3);
  gl4duScalef(5, 5, 5);
  glBindTexture(GL_TEXTURE_2D, tree_tId);
  assimpDrawScene();


glUniform4fv(glGetUniformLocation(tree_pId, "lumpos"), 1, lumpos);
  gl4duLoadIdentityf();
  gl4duLookAtf(_cam.x, landscape_y + 2.0, _cam.z, 
	       _cam.x - sin(_cam.theta), landscape_y + 2.0 - (ym - (_windowHeight >> 1)) / (GLfloat)_windowHeight, _cam.z - cos(_cam.theta), 
	       0.0, 1.0,0.0);
  gl4duTranslatef(-50, y+3 ,-3);
  gl4duScalef(5, 5, 5);
  glBindTexture(GL_TEXTURE_2D, tree_tId);
  assimpDrawScene();

glUniform4fv(glGetUniformLocation(tree_pId, "lumpos"), 1, lumpos);
  gl4duLoadIdentityf();
  gl4duLookAtf(_cam.x, landscape_y + 2.0, _cam.z, 
	       _cam.x - sin(_cam.theta), landscape_y + 2.0 - (ym - (_windowHeight >> 1)) / (GLfloat)_windowHeight, _cam.z - cos(_cam.theta), 
	       0.0, 1.0,0.0);
  gl4duTranslatef(-20, y+3 ,-3);
  gl4duScalef(5, 5, 5);
  glBindTexture(GL_TEXTURE_2D, tree_tId);
  assimpDrawScene();

glUniform4fv(glGetUniformLocation(tree_pId, "lumpos"), 1, lumpos);
  gl4duLoadIdentityf();
  gl4duLookAtf(_cam.x, landscape_y + 2.0, _cam.z, 
	       _cam.x - sin(_cam.theta), landscape_y + 2.0 - (ym - (_windowHeight >> 1)) / (GLfloat)_windowHeight, _cam.z - cos(_cam.theta), 
	       0.0, 1.0,0.0);
   y = heightMapAltitude(-20, -6);
  gl4duTranslatef(-20, y+3 ,-6);
  gl4duScalef(5, 5, 5);
  glBindTexture(GL_TEXTURE_2D, tree_tId);
  assimpDrawScene();
 

glUniform4fv(glGetUniformLocation(tree_pId, "lumpos"), 1, lumpos);
  gl4duLoadIdentityf();
  gl4duLookAtf(_cam.x, landscape_y + 2.0, _cam.z, 
	       _cam.x - sin(_cam.theta), landscape_y + 2.0 - (ym - (_windowHeight >> 1)) / (GLfloat)_windowHeight, _cam.z - cos(_cam.theta), 
	       0.0, 1.0,0.0);
   y = heightMapAltitude(-2, -10);
  gl4duTranslatef(-2, y+3 ,-10);
  gl4duScalef(5, 5, 5);
  glBindTexture(GL_TEXTURE_2D, tree_tId);
  assimpDrawScene();

 for(int i =0; i<5; i++){
    y = heightMapAltitude(-40-i, i-6);
   gl4duTranslatef(-40-i -20, y+3 ,i -6);
  //gl4duScalef(5, 5, 5);
  glBindTexture(GL_TEXTURE_2D, tree_tId);
  assimpDrawScene();
  }
}

/*!\brief libération des ressources utilisées */
static void quit(void) {
  freeNoiseTextures();
  if(_heightMap) {
    free(_heightMap);
    _heightMap = NULL;
  }
  if(_plasma_tId) {
    glDeleteTextures(1, &_plasma_tId);
    _plasma_tId = 0;
    glDeleteTextures(1, &_yellowred_tId);
    _yellowred_tId = 0;
    glDeleteTextures(1, &_terrain_tId);
    _terrain_tId = 0;
    glDeleteTextures(1, &_terrain_tId2);
    _terrain_tId2 =0;
  }
 /* if(_terrain_tId) {
    glDeleteTextures(1, &_terrain_tId);
    _terrain_tId = 0;
  }*/
  gl4duClean(GL4DU_ALL);
}

/*!\brief récupération de l'altitude y de la caméra selon son
 *  positionnement dans la heightMap. Cette fonction utilise les
 *  paramètres de scale pour faire la conversion d'un monde à
 *  l'autre. 
 *
 * Pour les étudiants souhaitant réaliser un projet type randonnée, il
 * faudra étendre cette fonction afin de récupérer la position y
 * exacte quand x et z ne tombent pas exactement sur un sommet mais se
 * trouvent entre 3 sommets. Indice, un triangle possède une normale,
 * la normale (a, b, c) donnent les coefficients de l'équation du plan
 * confondu avec le triangle par : ax + by + cz + d = 0. Reste plus
 * qu'à trouver d puis à chercher y pour x et z donnés.
 */
static GLfloat heightMapAltitude(GLfloat x, GLfloat z) {
  x = (_landscape_w >> 1) + (x / _landscape_scale_xz) * (_landscape_w / 2.0); 
  z = (_landscape_h >> 1) - (z / _landscape_scale_xz) * (_landscape_h / 2.0);
  if(x >= 0.0 && x < _landscape_w && z >= 0.0 && z < _landscape_h)
    return (2.0 * _heightMap[((int)x) + ((int)z) * _landscape_w] - 1) * _landscape_scale_y;
  return 0;
}


//=============================================================================================
// Mintaprogram: Zöld háromszög. Ervenyes 2019. osztol.
//
// A beadott program csak ebben a fajlban lehet, a fajl 1 byte-os ASCII karaktereket tartalmazhat, BOM kihuzando.
// Tilos:
// - mast "beincludolni", illetve mas konyvtarat hasznalni
// - faljmuveleteket vegezni a printf-et kiveve
// - Mashonnan atvett programresszleteket forrasmegjeloles nelkul felhasznalni es
// - felesleges programsorokat a beadott programban hagyni!!!!!!!
// - felesleges kommenteket a beadott programba irni a forrasmegjelolest kommentjeit kiveve
// ---------------------------------------------------------------------------------------------
// A feladatot ANSI C++ nyelvu forditoprogrammal ellenorizzuk, a Visual Studio-hoz kepesti elteresekrol
// es a leggyakoribb hibakrol (pl. ideiglenes objektumot nem lehet referencia tipusnak ertekul adni)
// a hazibeado portal ad egy osszefoglalot.
// ---------------------------------------------------------------------------------------------
// A feladatmegoldasokban csak olyan OpenGL fuggvenyek hasznalhatok, amelyek az oran a feladatkiadasig elhangzottak
// A keretben nem szereplo GLUT fuggvenyek tiltottak.
//
// NYILATKOZAT
// ---------------------------------------------------------------------------------------------
// Nev    : Csikos Patrik
// Neptun : E4MZUV
// ---------------------------------------------------------------------------------------------
// ezennel kijelentem, hogy a feladatot magam keszitettem, es ha barmilyen segitseget igenybe vettem vagy
// mas szellemi termeket felhasznaltam, akkor a forrast es az atvett reszt kommentekben egyertelmuen jeloltem.
// A forrasmegjeloles kotelme vonatkozik az eloadas foliakat es a targy oktatoi, illetve a
// grafhazi doktor tanacsait kiveve barmilyen csatornan (szoban, irasban, Interneten, stb.) erkezo minden egyeb
// informaciora (keplet, program, algoritmus, stb.). Kijelentem, hogy a forrasmegjelolessel atvett reszeket is ertem,
// azok helyessegere matematikai bizonyitast tudok adni. Tisztaban vagyok azzal, hogy az atvett reszek nem szamitanak
// a sajat kontribucioba, igy a feladat elfogadasarol a tobbi resz mennyisege es minosege alapjan szuletik dontes.
// Tudomasul veszem, hogy a forrasmegjeloles kotelmenek megsertese eseten a hazifeladatra adhato pontokat
// negativ elojellel szamoljak el es ezzel parhuzamosan eljaras is indul velem szemben.
//=============================================================================================
#include "framework.h"

//Vertex Shader
static const char *vertexShaderSource = R"(
#version 330
precision highp float;

layout(location = 0) in vec2 position;

uniform mat4 MVP;

void main()
{
	vec4 euk = vec4(position, 0, 1) * MVP;
    float z = sqrt(euk.x * euk.x + euk.y * euk.y + 1) + 1;
    gl_Position = vec4(euk.x / z, euk.y / z, 0, 1);
}

)";
//Fragment Shader
static const char *fragmentShaderSource = R"(
#version 330
precision highp float;

uniform vec3 color;

out vec4 fragmentColor;

void main()
{
	//fragmentColor = vec4(0, 1, 1, 1);
	fragmentColor = vec4(color, 1);
}

)";
GPUProgram gpuProgram;


const vec2 cameraSize = vec2(200, 200);
const float startCameraZoom = 1.0f;
const float atomR = 10.0f;
const float maxX = 6.0f; // *atomR * 2 = 80 max atom merete
const float maxY = maxX;
const float maxXWord = maxX * 100.0f;
const float maxYWord = maxXWord;
const unsigned char n = 20;
const unsigned char maxAtom = 8;
const unsigned char minAtom = 2;

//physics
const float k = 1000.0f; //Coulomb constant
const float A = 0.0001f; //Resistance constant
const int maxWeight = 10;
const int maxCharge = 50;


int genRandom(int min, int max) {
    return rand() % (max - min + 1) + min;
}

int genRandomExcept(int min, int max, int except) {
    int r = genRandom(min, max);
    while (r == except)
        r = genRandom(min, max);
    return r;
}

float distance(vec2 a, vec2 b) {
    return length(a - b);
}

class Camera2D {
    vec2 wCenter; // center in world coordinates
    vec2 wSize;   // width and height in world coordinates
public:
    Camera2D() : wCenter(0, 0), wSize(cameraSize) { }

    mat4 V() { return TranslateMatrix(-wCenter); }
    mat4 P() { return ScaleMatrix(vec2(2 / wSize.x, 2 / wSize.y)); }

    mat4 Vinv() { return TranslateMatrix(wCenter); }
    mat4 Pinv() { return ScaleMatrix(vec2(wSize.x / 2, wSize.y / 2)); }

    void Zoom(float s) { wSize = wSize * s; }
    void Pan(vec2 t) { wCenter = wCenter + t; }
};
Camera2D camera;

vec2 *drawCircle(float r, vec2 center, unsigned int n, vec2 *store) {
    store[0] = center;
    for (size_t i = 0; i < n - 2; i++) {
        store[i + 1] = vec2(center.x + r * cos(2 * M_PI * i / (n - 2)), center.y + r * sin(2 * M_PI * i / (n - 2)));
    }
    store[n - 1] = store[1];

    return store;
}

//felbontja irány és normálvektorra a vektort (s1 irányába)
vec4 splitF(vec2 F, vec2 s1){
    float x = dot(F, s1)/dot(s1, s1);
    vec2 Fd = x * s1;  // mozgás
    vec2 Fn = F - Fd;  // forgatás
    return vec4(Fd.x, Fd.y, Fn.x, Fn.y);
}

class atom{
    unsigned int vao;
    vec3 color;

    vec2 F = vec2(0, 0);
public:
    vec2 Fn = vec2(0, 0);
    vec2 Fd = vec2(0, 0);
    float weight;
    int Q;
    vec2 position; // távolság a molekula súlypontjától

    atom() {
        weight = float(genRandom(1, maxWeight));
        Q = -genRandom(-maxCharge, maxCharge);
        position = vec2(genRandom(-maxX, maxX) * atomR * 2, genRandom(-maxY, maxY) * atomR * 2);
        if (Q > 0) {
            color = vec3(Q * (1.0f / float(maxCharge)), 0.0f, 0.0f);
        } else {
            color = vec3(0.0f, 0.0f, Q * (-1.0f / float(maxCharge)));
        }
    }

    void calcCoulomb(atom a, vec2 s1, vec2 s2) {
        //coulomb
        float d = distance(position + s1, a.position + s2);
        float Fsize = (Q*a.Q / (d*d)) * k;
        F = F + Fsize * normalize((s1+position)-(s2+a.position));
    }

    void split(){
        vec4 splitBuff = splitF(F, position);
        Fd = vec2(splitBuff.x, splitBuff.y);
        Fn = vec2(splitBuff.z, splitBuff.w);
        F = vec2(0, 0);
    }

    void kozeg(float v, float szogsebesseg, vec2 Fv){
        if(length(Fv) != 0){
            F = F + ((A * v * v) * -Fv);
        }
        float keruleti = szogsebesseg * length(position);
        if(length(forgatonyomatek) != 0){
            F = F + ((A * keruleti * keruleti) * normalize(-forgatonyomatek));
        }
    }

    vec2 forgatonyomatek = vec2(0, 0);
    vec2 szogGYorsulas;
    void calcRes(float dt){
        forgatonyomatek = Fn * length(position);

        vec3 x = vec3(0, 0, 0);
        x = cross(vec3(Fn.x, Fn.y, 0.0f), vec3(position.y, position.x, 0.0f));

        szogGYorsulas = Fn / (length(position) * weight);
        if(x.z < 0){
            szogGYorsulas = -szogGYorsulas;
        }
    }

    void load(){
        vec2 kor[n];
        drawCircle(atomR, position, n, kor);

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        unsigned int positionBuffer;
        glGenBuffers(1, &positionBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * n, kor, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), (void *) 0);
        glEnableVertexAttribArray(0);
    }
    void draw() {
        gpuProgram.setUniform(color, "color");

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, n);
    }
    unsigned int vaox;
    void drawForce(vec2 s){
        vec2 lineForce[2];
        lineForce[0] = position;
        lineForce[1] = position + F;


        glGenVertexArrays(1, &vaox);
        glBindVertexArray(vaox);

        unsigned int positionBuffer;
        glGenBuffers(1, &positionBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * 2, lineForce, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), (void *) 0);
        glEnableVertexAttribArray(0);

        gpuProgram.setUniform(vec3(0, 0, 0), "color");
        glDrawArrays(GL_LINES, 0, 2);
    }
    void rotate(float angle){
        position.x = position.x * cos(angle) - position.y * sin(angle);
        position.y = position.x * sin(angle) + position.y * cos(angle);

        if(isnan(position.x)){
            position.x = 0;
        }
    }
};


class molekule{
    mat4 matrix = mat4(
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
    );

    unsigned int atomCount;
    atom *atoms;
    vec2 *connections;
    unsigned int *connectionVao;
    vec2 position; //súlypont pozíciója

    vec2 Fn = vec2(0, 0);
    vec2 Fd = vec2(0, 0);
    float v = 0;
    vec2 Fv = vec2(0, 0);
    vec2 Fsz = vec2(0, 0);
    float a = 0;
    float szogsebesseg = 0.0f;
    vec2 szogGyorsulas = vec2(0, 0);
    unsigned int weight = 0;

    vec2 sulypontCalc() {
        //calc weighted middle
        position = {0.0f, 0.0f};

        float weightSum = 0.0f;
        for (int i = 0; i < atomCount; ++i) {
            position = position + (atoms[i].position * atoms[i].weight);
            weightSum += atoms[i].weight;
        }

        position = position / weightSum;
        return position;
    }
    void loadConnection(atom a1, atom a2, unsigned int &vao) {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        vec2 line[2] = {a1.position, a2.position};

        unsigned int positionBuffer;
        glGenBuffers(1, &positionBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * n, line, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), (void *) 0);
        glEnableVertexAttribArray(0);
    }

public:
    molekule(){
       atomCount = genRandom(minAtom, maxAtom);
       atoms = new atom[atomCount];

       int chargeSum = 0;
       for (int i = 0; i < atomCount; i++) {
           atoms[i] = atom();
           weight = weight + atoms[i].weight;
           chargeSum += atoms[i].Q;
       }

       //Eltoljuk a molekulát, hogy a súlypont legyen a 0,0
       position = sulypontCalc();

        for (int i = 0; i < atomCount; ++i) {
            atoms[i].position = atoms[i].position - position;
        }

        position = {0.0f, 0.0f};

       chargeSum -= atoms[0].Q;
       atoms[0].Q = -chargeSum;

       connectionVao = new unsigned int[atomCount - 1];
       connections = new vec2[atomCount - 1];
       for (int i = 0; i < (atomCount - 1); i++) {
           connections[i] = vec2(i, genRandom(i + 1, atomCount - 1));
       }
    }
    void load(){
       for (int i = 0; i < atomCount; ++i) {
           atoms[i].load();
           if (i < atomCount - 1) {
               loadConnection(atoms[int(connections[i].x)],
                              atoms[int(connections[i].y)], connectionVao[i]);
           }
       }
    }
    void draw(){
        gpuProgram.setUniform(matrix * camera.V() * camera.P(), "MVP");
        for (int i = 0; i < atomCount; ++i) {
            atoms[i].draw();
            //atoms[i].drawForce(position);

            if (i < atomCount - 1) {
                gpuProgram.setUniform(vec3(1, 1, 1), "color");
                glBindVertexArray(connectionVao[i]);
                glDrawArrays(GL_LINES, 0, 2);
            }
        }
    }
    void move(vec2 diff){
        matrix = matrix * TranslateMatrix(vec3(diff.x, diff.y, 0));
        position = position + diff;
    }
    void rotate(float angle){
        for(int i = 0; i < atomCount; i++){
            atoms[i].rotate(angle);
        }
        matrix = matrix *
                TranslateMatrix(vec3(-position.x, -position.y, 0)) *
                RotationMatrix(angle, vec3(0, 0, 1)) *
                TranslateMatrix(vec3(position.x, position.y, 0));
    }

    void calcMovement(float dt){
        Fd = {0.0f, 0.0f};

        for (int i = 0; i < atomCount; ++i) {
            atoms[i].split();
            Fd = Fd + atoms[i].Fd;
            atoms[i].calcRes(dt);
            szogGyorsulas = szogGyorsulas + atoms[i].szogGYorsulas;
        }


        szogsebesseg = szogsebesseg + length(szogGyorsulas) * dt;
        szogsebesseg = szogsebesseg * (M_PI/180.0f);

        a = length(Fd) / weight;
        v = v + dt * a;
        if(a > 0){
            Fv = normalize(Fd);
        }
    }

    void calcFs(molekule m2, float dt){
        for (int i = 0; i < atomCount; ++i) {
            for (int j = 0; j < m2.atomCount; ++j) {
                atoms[i].calcCoulomb(m2.atoms[j], position, m2.position);
                atoms[i].kozeg(v, szogsebesseg, Fv);
            }
        }
    }

    void moveByForce(float dt){
        //move
        move(Fv * v * dt);

        //rotate
        rotate(szogsebesseg * dt);

    }
};

molekule* ms;
unsigned int msCount = 0;

// Initialization, create an OpenGL context
void onInitialization() {

    glLineWidth(3);
    gpuProgram.create(vertexShaderSource, fragmentShaderSource, "fragmentColor");

    camera.Zoom(startCameraZoom);

    msCount += 2;
    ms = (molekule*) malloc(sizeof(molekule) * msCount);

    ms[msCount - 2] = molekule();
    vec2 pos = {static_cast<float>(genRandom(-maxXWord, maxXWord)), static_cast<float>(genRandom(-maxXWord, maxXWord))};
    ms[msCount - 2].move(pos);
    ms[msCount - 2].load();

    ms[msCount - 1] = molekule();
    pos = {static_cast<float>(genRandom(-maxXWord, maxXWord)), static_cast<float>(genRandom(-maxXWord, maxXWord))};
    ms[msCount - 1].move(pos);
    ms[msCount - 1].load();

    glutPostRedisplay();
}

// Window has become invalid: Redraw
void onDisplay() {
    glClearColor(0.3f, 0.3f, 0.3f, 0.3f);     // background color
    glClear(GL_COLOR_BUFFER_BIT); // clear frame buffer

    gpuProgram.Use();

    for (int i = 0; i < msCount; ++i) {
        ms[i].draw();
    }

    glutSwapBuffers();
}


bool animate = true;
// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
    const float step = 0.1f;

    switch (key) {
        case ' ': {
            msCount += 2;
            ms = (molekule*) realloc(ms, sizeof(molekule) * msCount);

            ms[msCount - 2] = molekule();
            vec2 pos = {static_cast<float>(genRandom(-maxXWord, maxXWord)), static_cast<float>(genRandom(-maxXWord, maxXWord))};
            ms[msCount - 2].move(pos);
            ms[msCount - 2].load();

            ms[msCount - 1] = molekule();
            pos = {static_cast<float>(genRandom(-maxXWord, maxXWord)), static_cast<float>(genRandom(-maxXWord, maxXWord))};
            ms[msCount - 1].move(pos);
            ms[msCount - 1].load();


            break;
        }
        case 'q': {
            exit(0);
            break;
        }
        case 's': {
            camera.Pan(vec2(step, 0));
            break;
        }
        case 'd': {
            camera.Pan(vec2(-step, 0));
            break;
        }
        case 'x': {
            camera.Pan(vec2(0, -step));
            break;
        }
        case 'e': {
            camera.Pan(vec2(0, step));
            break;
        }

        case 'k': {
            camera.Zoom(0.8f);
            break;
        }
        case 'l': {
            camera.Zoom(1.2f);
            break;
        }
        case 'r': {
            animate = !animate;
            printf("animate: %d\n", animate);
            break;
        }
    }

    glutPostRedisplay();
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {
}

// Move mouse with key pressed
void onMouseMotion(int pX,
                   int pY) {    // pX, pY are the pixel coordinates of the cursor in the coordinate system of the operation system

}

// Mouse click event
void onMouse(int button, int state, int pX,
             int pY) { // pX, pY are the pixel coordinates of the cursor in the coordinate system of the operation system

}

// Idle event indicating that some time elapsed: do animation here
float prevTime = 0;
void onIdle() {
    float time = glutGet(GLUT_ELAPSED_TIME) * 0.001f; //seconds

    for (float t = prevTime; t < time; t = t+0.01f) {
        if (animate){
            for (int i = 0; i < msCount; ++i) {
                for (int j = 0; j < msCount; ++j) {
                    if(i != j){
                        ms[i].calcFs(ms[j], 0.01f);
                    }
                }
            }

            for (int i = 0; i < msCount; ++i) {
                ms[i].calcMovement(0.01f);
                ms[i].moveByForce(0.01f);
            }
        }
    }
    prevTime = time;
    glutPostRedisplay();
}
// game.c - Complete 3D Driving Game with Physics
// Compile: gcc game.c -o game.exe -static -lglfw3 -lglew32 -lopengl32 -lgdi32 -lm

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define WIDTH 1280
#define HEIGHT 720
#define WORLD_SIZE 50.0f
#define PI 3.14159265f

// ========== VECTORS ==========
typedef struct { float x, y, z; } Vec3;
Vec3 vec3(float x, float y, float z) { Vec3 v = {x,y,z}; return v; }
Vec3 vec3_add(Vec3 a, Vec3 b) { return vec3(a.x+b.x, a.y+b.y, a.z+b.z); }
Vec3 vec3_sub(Vec3 a, Vec3 b) { return vec3(a.x-b.x, a.y-b.y, a.z-b.z); }
Vec3 vec3_scale(Vec3 v, float s) { return vec3(v.x*s, v.y*s, v.z*s); }
Vec3 vec3_normalize(Vec3 v) { float l = sqrt(v.x*v.x+v.y*v.y+v.z*v.z); return l>0?vec3(v.x/l,v.y/l,v.z/l):vec3(0,0,0); }
float vec3_dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
Vec3 vec3_cross(Vec3 a, Vec3 b) { return vec3(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x); }
float vec3_dist(Vec3 a, Vec3 b) { return sqrt(pow(a.x-b.x,2)+pow(a.y-b.y,2)+pow(a.z-b.z,2)); }

// ========== STRUCTURES ==========
typedef struct { float pos[3]; float normal[3]; float uv[2]; } Vertex;

typedef struct {
    unsigned int vao, vbo, ebo;
    int vertex_count;
    int index_count;
    unsigned int texture;
    Vec3 pos, rot, scale;
} Mesh;

typedef struct {
    Vec3 pos;
    Vec3 front, up, right;
    float yaw, pitch;
    float distance;
} Camera;

typedef struct {
    Vec3 pos, velocity;
    float yaw;
    float speed, max_speed, acceleration, braking, turn_speed;
    float mass;
} Car;

// ========== PHYSICS ENGINE ==========
typedef struct {
    Vec3 pos;
    Vec3 size;
    bool is_static;
    Vec3 velocity;
    float mass;
} PhysicsBody;

PhysicsBody physics_bodies[100];
int physics_body_count = 0;

void physics_add_body(PhysicsBody body) {
    if (physics_body_count < 100) {
        physics_bodies[physics_body_count++] = body;
    }
}

bool physics_check_collision(PhysicsBody a, PhysicsBody b) {
    return (fabs(a.pos.x - b.pos.x) < (a.size.x/2 + b.size.x/2) &&
            fabs(a.pos.y - b.pos.y) < (a.size.y/2 + b.size.y/2) &&
            fabs(a.pos.z - b.pos.z) < (a.size.z/2 + b.size.z/2));
}

void physics_resolve_collision(PhysicsBody* a, PhysicsBody* b) {
    Vec3 overlap = vec3(
        (a->size.x/2 + b->size.x/2) - fabs(a->pos.x - b->pos.x),
        (a->size.y/2 + b->size.y/2) - fabs(a->pos.y - b->pos.y),
        (a->size.z/2 + b->size.z/2) - fabs(a->pos.z - b->pos.z)
    );
    
    // Push apart
    if (overlap.x < overlap.y && overlap.x < overlap.z) {
        float sign = (a->pos.x < b->pos.x) ? -1 : 1;
        a->pos.x += sign * overlap.x * 0.5f;
        a->velocity.x = 0;
    } else if (overlap.y < overlap.x && overlap.y < overlap.z) {
        float sign = (a->pos.y < b->pos.y) ? -1 : 1;
        a->pos.y += sign * overlap.y * 0.5f;
        a->velocity.y = 0;
    } else {
        float sign = (a->pos.z < b->pos.z) ? -1 : 1;
        a->pos.z += sign * overlap.z * 0.5f;
        a->velocity.z = 0;
    }
}

void update_car_physics(Car* car, float dt, PhysicsBody* obstacles, int obstacle_count) {
    // Gravity
    car->velocity.y -= 9.8f * dt;
    
    // Apply velocity
    car->pos.x += car->velocity.x * dt;
    car->pos.y += car->velocity.y * dt;
    car->pos.z += car->velocity.z * dt;
    
    // Ground collision
    if (car->pos.y < 0.5f) {
        car->pos.y = 0.5f;
        car->velocity.y = 0;
    }
    
    // World bounds
    if (car->pos.x > WORLD_SIZE-2) { car->pos.x = WORLD_SIZE-2; car->velocity.x = 0; }
    if (car->pos.x < -WORLD_SIZE+2) { car->pos.x = -WORLD_SIZE+2; car->velocity.x = 0; }
    if (car->pos.z > WORLD_SIZE-2) { car->pos.z = WORLD_SIZE-2; car->velocity.z = 0; }
    if (car->pos.z < -WORLD_SIZE+2) { car->pos.z = -WORLD_SIZE+2; car->velocity.z = 0; }
    
    // Obstacle collision
    PhysicsBody car_body = {car->pos, vec3(1.0f, 0.5f, 0.8f), false, {0,0,0}, 1.0f};
    for (int i = 0; i < obstacle_count; i++) {
        if (physics_check_collision(car_body, obstacles[i])) {
            physics_resolve_collision(&car_body, &obstacles[i]);
            car->pos = car_body.pos;
            car->velocity.x *= 0.5f;
            car->velocity.z *= 0.5f;
        }
    }
}

// ========== SHADERS ==========
const char* vertex_shader =
    "#version 330 core\n"
    "layout(location=0) in vec3 aPos;\n"
    "layout(location=1) in vec3 aNormal;\n"
    "layout(location=2) in vec2 aUV;\n"
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "out vec3 FragPos;\n"
    "out vec3 Normal;\n"
    "out vec2 UV;\n"
    "void main() {\n"
    "    FragPos = vec3(model * vec4(aPos, 1.0));\n"
    "    Normal = mat3(transpose(inverse(model))) * aNormal;\n"
    "    UV = aUV;\n"
    "    gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
    "}\n";

const char* fragment_shader =
    "#version 330 core\n"
    "in vec3 FragPos;\n"
    "in vec3 Normal;\n"
    "in vec2 UV;\n"
    "uniform vec3 lightPos;\n"
    "uniform vec3 lightColor;\n"
    "uniform vec3 viewPos;\n"
    "uniform vec3 objectColor;\n"
    "uniform int hasTexture;\n"
    "uniform sampler2D texture1;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    vec3 norm = normalize(Normal);\n"
    "    vec3 lightDir = normalize(lightPos - FragPos);\n"
    "    float diff = max(dot(norm, lightDir), 0.0);\n"
    "    vec3 diffuse = diff * lightColor;\n"
    "    float ambientStrength = 0.3;\n"
    "    vec3 ambient = ambientStrength * lightColor;\n"
    "    vec3 viewDir = normalize(viewPos - FragPos);\n"
    "    vec3 reflectDir = reflect(-lightDir, norm);\n"
    "    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);\n"
    "    vec3 specular = spec * lightColor;\n"
    "    vec3 result = (ambient + diffuse + specular) * objectColor;\n"
    "    if (hasTexture == 1) {\n"
    "        vec4 texColor = texture(texture1, UV);\n"
    "        result *= texColor.rgb;\n"
    "    }\n"
    "    FragColor = vec4(result, 1.0);\n"
    "}\n";

// ========== SHADER COMPILER ==========
unsigned int compile_shader(const char* source, GLenum type) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    int success; char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) { glGetShaderInfoLog(shader, 512, NULL, infoLog); printf("Shader error: %s\n", infoLog); }
    return shader;
}

unsigned int create_program() {
    unsigned int vs = compile_shader(vertex_shader, GL_VERTEX_SHADER);
    unsigned int fs = compile_shader(fragment_shader, GL_FRAGMENT_SHADER);
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    int success; char infoLog[512];
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) { glGetProgramInfoLog(prog, 512, NULL, infoLog); printf("Link error: %s\n", infoLog); }
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

// ========== MESH BUILDER ==========
void create_plane_mesh(Mesh* m, float size) {
    Vertex verts[] = {
        {{-size,0,-size},{0,1,0},{0,0}},
        {{size,0,-size},{0,1,0},{1,0}},
        {{size,0,size},{0,1,0},{1,1}},
        {{-size,0,size},{0,1,0},{0,1}}
    };
    unsigned int indices[] = {0,1,2, 0,2,3};
    m->vertex_count = 4; m->index_count = 6;
    m->pos = vec3(0,0,0); m->rot = vec3(0,0,0); m->scale = vec3(1,1,1);
    glGenVertexArrays(1, &m->vao); glGenBuffers(1, &m->vbo); glGenBuffers(1, &m->ebo);
    glBindVertexArray(m->vao);
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6*sizeof(float))); glEnableVertexAttribArray(2);
    m->texture = 0;
}

void create_car_mesh(Mesh* m) {
    // Simple car using triangles directly
    Vertex verts[200]; int vc = 0;
    unsigned int indices[300]; int ic = 0;
    
    // Car body (simplified)
    float body_w = 0.9f, body_h = 0.3f, body_d = 0.5f;
    float cabin_w = 0.7f, cabin_h = 0.25f, cabin_d = 0.3f;
    
    // Body vertices (box)
    Vec3 p[8] = {
        vec3(-body_w, -body_h/2, -body_d),
        vec3(body_w, -body_h/2, -body_d),
        vec3(body_w, body_h/2, -body_d),
        vec3(-body_w, body_h/2, -body_d),
        vec3(-body_w, -body_h/2, body_d),
        vec3(body_w, -body_h/2, body_d),
        vec3(body_w, body_h/2, body_d),
        vec3(-body_w, body_h/2, body_d)
    };
    
    // Add box function
    void add_quad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 normal, float r, float g, float bl) {
        Vertex v[4] = {
            {{a.x,a.y,a.z},{normal.x,normal.y,normal.z},{0,0}},
            {{b.x,b.y,b.z},{normal.x,normal.y,normal.z},{1,0}},
            {{c.x,c.y,c.z},{normal.x,normal.y,normal.z},{1,1}},
            {{d.x,d.y,d.z},{normal.x,normal.y,normal.z},{0,1}}
        };
        for (int i = 0; i < 4; i++) {
            verts[vc] = v[i];
            vc++;
        }
        int base = vc-4;
        indices[ic++] = base; indices[ic++] = base+1; indices[ic++] = base+2;
        indices[ic++] = base; indices[ic++] = base+2; indices[ic++] = base+3;
    }
    
    // Body faces
    Vec3 normals[6] = {
        {0,0,-1},{0,0,1},{0,-1,0},{0,1,0},{-1,0,0},{1,0,0}
    };
    int face_inds[6][4] = {{0,1,2,3},{4,5,6,7},{0,1,5,4},{2,3,7,6},{0,3,7,4},{1,2,6,5}};
    float color = 0.8f;
    for (int f = 0; f < 6; f++) {
        Vec3 a = p[face_inds[f][0]];
        Vec3 b = p[face_inds[f][1]];
        Vec3 c = p[face_inds[f][2]];
        Vec3 d = p[face_inds[f][3]];
        add_quad(a,b,c,d,normals[f],color,0.2f,0.1f);
    }
    
    // Cabin (simple box on top)
    Vec3 cp[8] = {
        vec3(-cabin_w, body_h/2, -cabin_d),
        vec3(cabin_w, body_h/2, -cabin_d),
        vec3(cabin_w, body_h/2+cabin_h, -cabin_d),
        vec3(-cabin_w, body_h/2+cabin_h, -cabin_d),
        vec3(-cabin_w, body_h/2, cabin_d),
        vec3(cabin_w, body_h/2, cabin_d),
        vec3(cabin_w, body_h/2+cabin_h, cabin_d),
        vec3(-cabin_w, body_h/2+cabin_h, cabin_d)
    };
    int face_inds2[6][4] = {{0,1,2,3},{4,5,6,7},{0,1,5,4},{2,3,7,6},{0,3,7,4},{1,2,6,5}};
    for (int f = 0; f < 6; f++) {
        Vec3 a = cp[face_inds2[f][0]];
        Vec3 b = cp[face_inds2[f][1]];
        Vec3 c = cp[face_inds2[f][2]];
        Vec3 d = cp[face_inds2[f][3]];
        add_quad(a,b,c,d,normals[f],0.3f,0.6f,0.9f);
    }
    
    m->vertex_count = vc; m->index_count = ic;
    m->pos = vec3(0,0,0); m->rot = vec3(0,0,0); m->scale = vec3(0.5f,0.5f,0.5f);
    glGenVertexArrays(1, &m->vao); glGenBuffers(1, &m->vbo); glGenBuffers(1, &m->ebo);
    glBindVertexArray(m->vao);
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferData(GL_ARRAY_BUFFER, vc * sizeof(Vertex), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ic * sizeof(unsigned int), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6*sizeof(float))); glEnableVertexAttribArray(2);
    m->texture = 0;
}

void create_tree_mesh(Mesh* m) {
    // Tree trunk (box)
    Vertex verts[200]; int vc = 0;
    unsigned int indices[300]; int ic = 0;
    
    Vec3 p[8] = {
        vec3(-0.1f, -0.5f, -0.1f),
        vec3(0.1f, -0.5f, -0.1f),
        vec3(0.1f, 0.5f, -0.1f),
        vec3(-0.1f, 0.5f, -0.1f),
        vec3(-0.1f, -0.5f, 0.1f),
        vec3(0.1f, -0.5f, 0.1f),
        vec3(0.1f, 0.5f, 0.1f),
        vec3(-0.1f, 0.5f, 0.1f)
    };
    
    void add_quad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 normal, float r, float g, float bl) {
        Vertex v[4] = {
            {{a.x,a.y,a.z},{normal.x,normal.y,normal.z},{0,0}},
            {{b.x,b.y,b.z},{normal.x,normal.y,normal.z},{1,0}},
            {{c.x,c.y,c.z},{normal.x,normal.y,normal.z},{1,1}},
            {{d.x,d.y,d.z},{normal.x,normal.y,normal.z},{0,1}}
        };
        for (int i = 0; i < 4; i++) {
            verts[vc] = v[i];
            vc++;
        }
        int base = vc-4;
        indices[ic++] = base; indices[ic++] = base+1; indices[ic++] = base+2;
        indices[ic++] = base; indices[ic++] = base+2; indices[ic++] = base+3;
    }
    
    Vec3 normals[6] = {
        {0,0,-1},{0,0,1},{0,-1,0},{0,1,0},{-1,0,0},{1,0,0}
    };
    int face_inds[6][4] = {{0,1,2,3},{4,5,6,7},{0,1,5,4},{2,3,7,6},{0,3,7,4},{1,2,6,5}};
    for (int f = 0; f < 6; f++) {
        Vec3 a = p[face_inds[f][0]];
        Vec3 b = p[face_inds[f][1]];
        Vec3 c = p[face_inds[f][2]];
        Vec3 d = p[face_inds[f][3]];
        add_quad(a,b,c,d,normals[f],0.4f,0.3f,0.1f);
    }
    
    // Canopy (sphere approximation - just a box for simplicity)
    Vec3 cp[8] = {
        vec3(-0.4f, 0.5f, -0.4f),
        vec3(0.4f, 0.5f, -0.4f),
        vec3(0.4f, 1.0f, -0.4f),
        vec3(-0.4f, 1.0f, -0.4f),
        vec3(-0.4f, 0.5f, 0.4f),
        vec3(0.4f, 0.5f, 0.4f),
        vec3(0.4f, 1.0f, 0.4f),
        vec3(-0.4f, 1.0f, 0.4f)
    };
    for (int f = 0; f < 6; f++) {
        Vec3 a = cp[face_inds[f][0]];
        Vec3 b = cp[face_inds[f][1]];
        Vec3 c = cp[face_inds[f][2]];
        Vec3 d = cp[face_inds[f][3]];
        add_quad(a,b,c,d,normals[f],0.1f,0.6f,0.1f);
    }
    
    m->vertex_count = vc; m->index_count = ic;
    m->pos = vec3(0,0,0); m->rot = vec3(0,0,0); m->scale = vec3(1,1,1);
    glGenVertexArrays(1, &m->vao); glGenBuffers(1, &m->vbo); glGenBuffers(1, &m->ebo);
    glBindVertexArray(m->vao);
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferData(GL_ARRAY_BUFFER, vc * sizeof(Vertex), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ic * sizeof(unsigned int), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6*sizeof(float))); glEnableVertexAttribArray(2);
    m->texture = 0;
}

// ========== MATRIX MATH ==========
void mat4_identity(float* m) {
    memset(m, 0, 16*sizeof(float));
    m[0]=m[5]=m[10]=m[15]=1;
}

void mat4_translate(float* m, Vec3 v) {
    mat4_identity(m);
    m[12] = v.x; m[13] = v.y; m[14] = v.z;
}

void mat4_rotate_y(float* m, float angle) {
    mat4_identity(m);
    float c = cos(angle), s = sin(angle);
    m[0]=c; m[2]=s; m[8]=-s; m[10]=c;
}

void mat4_rotate_x(float* m, float angle) {
    mat4_identity(m);
    float c = cos(angle), s = sin(angle);
    m[5]=c; m[6]=-s; m[9]=s; m[10]=c;
}

void mat4_scale(float* m, Vec3 v) {
    mat4_identity(m);
    m[0]=v.x; m[5]=v.y; m[10]=v.z;
}

void mat4_mul(float* a, float* b, float* out) {
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) {
        out[i*4+j] = a[i*4]*b[j] + a[i*4+1]*b[4+j] + a[i*4+2]*b[8+j] + a[i*4+3]*b[12+j];
    }
}

void mat4_perspective(float* m, float fov, float aspect, float near, float far) {
    float tan_half_fov = tan(fov/2);
    memset(m, 0, 16*sizeof(float));
    m[0] = 1/(aspect*tan_half_fov);
    m[5] = 1/tan_half_fov;
    m[10] = -(far+near)/(far-near);
    m[11] = -1;
    m[14] = -(2*far*near)/(far-near);
}

void mat4_lookat(float* m, Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = vec3_normalize(vec3_sub(center, eye));
    Vec3 s = vec3_normalize(vec3_cross(f, up));
    Vec3 u = vec3_cross(s, f);
    mat4_identity(m);
    m[0]=s.x; m[4]=s.y; m[8]=s.z; m[12]=-vec3_dot(s,eye);
    m[1]=u.x; m[5]=u.y; m[9]=u.z; m[13]=-vec3_dot(u,eye);
    m[2]=-f.x; m[6]=-f.y; m[10]=-f.z; m[14]=vec3_dot(f,eye);
}

// ========== CAR PHYSICS ==========
void update_car(Car* car, float dt, int forward, int backward, int left, int right) {
    if (forward) car->speed += car->acceleration * dt;
    else if (backward) car->speed -= car->braking * dt;
    else car->speed *= 0.98f;
    
    if (car->speed > car->max_speed) car->speed = car->max_speed;
    if (car->speed < -car->max_speed/2) car->speed = -car->max_speed/2;
    
    if (fabs(car->speed) > 0.1f) {
        float turn = 0;
        if (left) turn = car->turn_speed * dt * (car->speed > 0 ? 1 : -1);
        if (right) turn = -car->turn_speed * dt * (car->speed > 0 ? 1 : -1);
        car->yaw += turn;
    }
    
    car->pos.x += sin(car->yaw) * car->speed * dt;
    car->pos.z += cos(car->yaw) * car->speed * dt;
    
    if (car->pos.x > WORLD_SIZE-1) car->pos.x = -WORLD_SIZE+1;
    if (car->pos.x < -WORLD_SIZE+1) car->pos.x = WORLD_SIZE-1;
    if (car->pos.z > WORLD_SIZE-1) car->pos.z = -WORLD_SIZE+1;
    if (car->pos.z < -WORLD_SIZE+1) car->pos.z = WORLD_SIZE-1;
}

// ========== CAMERA ==========
void update_camera(Camera* cam, Car* car) {
    cam->yaw = car->yaw;
    cam->pitch = 0.3f;
    cam->distance = 5.0f;
    
    Vec3 offset = vec3(sin(cam->yaw)*cam->distance, 2.5f, cos(cam->yaw)*cam->distance);
    cam->pos = vec3_add(car->pos, offset);
    
    Vec3 target = car->pos;
    target.y += 0.5f;
    cam->front = vec3_normalize(vec3_sub(target, cam->pos));
    Vec3 world_up = vec3(0,1,0);
    cam->right = vec3_normalize(vec3_cross(cam->front, world_up));
    cam->up = vec3_cross(cam->right, cam->front);
}

// ========== RENDERER ==========
void render_mesh(Mesh* m, unsigned int shader, float* view, float* proj, Vec3 lightPos, Vec3 viewPos, Vec3 color) {
    float model[16], temp[16], temp2[16];
    mat4_identity(model);
    
    float trans[16]; mat4_translate(trans, m->pos);
    mat4_mul(trans, model, temp);
    memcpy(model, temp, 16*sizeof(float));
    
    float rotx[16], roty[16];
    mat4_rotate_x(rotx, m->rot.x);
    mat4_rotate_y(roty, m->rot.y);
    mat4_mul(rotx, model, temp);
    mat4_mul(roty, temp, model);
    
    float scale[16]; mat4_scale(scale, m->scale);
    mat4_mul(scale, model, temp);
    memcpy(model, temp, 16*sizeof(float));
    
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, model);
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, proj);
    glUniform3f(glGetUniformLocation(shader, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
    glUniform3f(glGetUniformLocation(shader, "lightColor"), 1, 1, 1);
    glUniform3f(glGetUniformLocation(shader, "viewPos"), viewPos.x, viewPos.y, viewPos.z);
    glUniform3f(glGetUniformLocation(shader, "objectColor"), color.x, color.y, color.z);
    glUniform1i(glGetUniformLocation(shader, "hasTexture"), 0);
    
    glBindVertexArray(m->vao);
    glDrawElements(GL_TRIANGLES, m->index_count, GL_UNSIGNED_INT, 0);
}

// ========== MAIN ==========
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "3D Driving Game", NULL, NULL);
    if (!window) { printf("Failed to create window\n"); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    
    glewExperimental = true;
    if (glewInit() != GLEW_OK) { printf("Failed to init GLEW\n"); return -1; }
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glClearColor(0.1f, 0.2f, 0.4f, 1.0f);
    
    unsigned int shader = create_program();
    
    Mesh ground; create_plane_mesh(&ground, WORLD_SIZE);
    ground.pos = vec3(0, -0.5f, 0);
    
    Mesh car; create_car_mesh(&car);
    
    Mesh trees[30];
    for (int i = 0; i < 30; i++) {
        create_tree_mesh(&trees[i]);
        trees[i].pos = vec3((float)rand()/RAND_MAX*WORLD_SIZE*2-WORLD_SIZE, 0, (float)rand()/RAND_MAX*WORLD_SIZE*2-WORLD_SIZE);
        trees[i].scale = vec3(0.5 + (float)rand()/RAND_MAX * 0.5, 0.5 + (float)rand()/RAND_MAX * 0.5, 0.5 + (float)rand()/RAND_MAX * 0.5);
        if (sqrt(trees[i].pos.x*trees[i].pos.x + trees[i].pos.z*trees[i].pos.z) < 5) {
            trees[i].pos.x += 10; trees[i].pos.z += 10;
        }
    }
    
    Car car_phys = {0};
    car_phys.pos = vec3(0, 0.5f, 0);
    car_phys.max_speed = 10.0f;
    car_phys.acceleration = 5.0f;
    car_phys.braking = 8.0f;
    car_phys.turn_speed = 2.0f;
    
    Camera cam = {0};
    cam.distance = 5.0f;
    cam.pitch = 0.3f;
    
    Vec3 lightPos = vec3(10, 20, 10);
    float view[16], proj[16];
    mat4_perspective(proj, PI/3, (float)WIDTH/HEIGHT, 0.1f, 100.0f);
    
    double last_time = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double current_time = glfwGetTime();
        float dt = (float)(current_time - last_time);
        last_time = current_time;
        if (dt > 0.05f) dt = 0.05f;
        
        int forward = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        int backward = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        int left = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        int right = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
        
        update_car(&car_phys, dt, forward, backward, left, right);
        car.pos = car_phys.pos;
        car.rot.y = car_phys.yaw;
        
        update_camera(&cam, &car_phys);
        mat4_lookat(view, cam.pos, car_phys.pos, vec3(0,1,0));
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        render_mesh(&ground, shader, view, proj, lightPos, cam.pos, vec3(0.2f, 0.5f, 0.2f));
        
        for (int i = 0; i < 30; i++) {
            render_mesh(&trees[i], shader, view, proj, lightPos, cam.pos, vec3(0.4f, 0.6f, 0.2f));
        }
        
        render_mesh(&car, shader, view, proj, lightPos, cam.pos, vec3(0.8f, 0.2f, 0.1f));
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

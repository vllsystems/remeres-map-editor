#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
static void* rmeGetGLProc(const char* name) {
	void* p = (void*)wglGetProcAddress(name);
	if (p == 0 || p == (void*)0x1 || p == (void*)0x2 || p == (void*)0x3 || p == (void*)-1) {
		static HMODULE gl = LoadLibraryA("opengl32.dll");
		p = (void*)GetProcAddress(gl, name);
	}
	return p;
}
#else
	#include <dlfcn.h>
static void* rmeGetGLProc(const char* name) {
	static void* lib = dlopen("libGL.so", RTLD_LAZY);
	return lib ? dlsym(lib, name) : nullptr;
}
#endif

#include <glad/glad.h>
#include "main.h"
#include "gl_renderer.h"

static const char* vertSrc = R"(  
#version 330  
layout(location=0) in vec2 aPos;  
layout(location=1) in vec2 aUV;  
layout(location=2) in vec4 aColor;  
uniform mat4 uProjection;  
out vec2 vUV;  
out vec4 vColor;  
void main(){  
	gl_Position = uProjection * vec4(aPos, 0.0, 1.0);  
	vUV = aUV;  
	vColor = aColor;  
}  
)";

static const char* fragSrc = R"(
#version 330
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uTexture;
uniform int uUseTexture;
out vec4 FragColor;
void main(){
	if(uUseTexture != 0)
		FragColor = texture(uTexture, vUV) * vColor;
	else
		FragColor = vColor;
}
)";

void GLRenderer::init() {
	if (initialized) {
		return;
	}

	gladLoadGLLoader((GLADloadproc)rmeGetGLProc);

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &vertSrc, nullptr);
	glCompileShader(vs);

	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &fragSrc, nullptr);
	glCompileShader(fs);

	program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);

	glDeleteShader(vs);
	glDeleteShader(fs);

	loc_projection = glGetUniformLocation(program, "uProjection");
	loc_useTexture = glGetUniformLocation(program, "uUseTexture");
	loc_texture = glGetUniformLocation(program, "uTexture");

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, r));

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	initialized = true;
}

void GLRenderer::shutdown() {
	if (!initialized) {
		return;
	}
	if (program) {
		glDeleteProgram(program);
		program = 0;
	}
	if (vbo) {
		glDeleteBuffers(1, &vbo);
		vbo = 0;
	}
	if (vao) {
		glDeleteVertexArrays(1, &vao);
		vao = 0;
	}
	initialized = false;
}

void GLRenderer::setOrtho(float left, float right, float bottom, float top) {
	float m[16] = { 0 };
	m[0] = 2.0f / (right - left);
	m[5] = 2.0f / (top - bottom);
	m[10] = -1.0f;
	m[12] = -(right + left) / (right - left);
	m[13] = -(top + bottom) / (top - bottom);
	m[15] = 1.0f;

	glUseProgram(program);
	glUniformMatrix4fv(loc_projection, 1, GL_FALSE, m);
	glUseProgram(0);
}

void GLRenderer::flushBatch() {
	if (batch.empty() || !initialized) {
		return;
	}

	glUseProgram(program);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, batch.size() * sizeof(Vertex), batch.data(), GL_DYNAMIC_DRAW);

	if (current_texture != 0) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, current_texture);
		glUniform1i(loc_useTexture, 1);
		glUniform1i(loc_texture, 0);
	} else {
		glUniform1i(loc_useTexture, 0);
	}

	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)batch.size());

	glBindVertexArray(0);
	glUseProgram(0);
	batch.clear();
}

void GLRenderer::drawTexturedQuad(float x, float y, float w, float h, GLuint textureId, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	if (current_texture != textureId && !batch.empty()) {
		flushBatch();
	}
	current_texture = textureId;

	Vertex v0 = { x, y, 0, 0, r, g, b, a };
	Vertex v1 = { x + w, y, 1, 0, r, g, b, a };
	Vertex v2 = { x + w, y + h, 1, 1, r, g, b, a };
	Vertex v3 = { x, y + h, 0, 1, r, g, b, a };

	batch.push_back(v0);
	batch.push_back(v1);
	batch.push_back(v2);
	batch.push_back(v0);
	batch.push_back(v2);
	batch.push_back(v3);
}

void GLRenderer::drawColoredQuad(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	if (current_texture != 0 && !batch.empty()) {
		flushBatch();
	}
	current_texture = 0;

	Vertex v0 = { x, y, 0, 0, r, g, b, a };
	Vertex v1 = { x + w, y, 0, 0, r, g, b, a };
	Vertex v2 = { x + w, y + h, 0, 0, r, g, b, a };
	Vertex v3 = { x, y + h, 0, 0, r, g, b, a };

	batch.push_back(v0);
	batch.push_back(v1);
	batch.push_back(v2);
	batch.push_back(v0);
	batch.push_back(v2);
	batch.push_back(v3);
}

void GLRenderer::drawRect(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float lineWidth) {
	flushBatch();
	Vertex verts[5] = {
		{ x, y, 0, 0, r, g, b, a },
		{ x + w, y, 0, 0, r, g, b, a },
		{ x + w, y + h, 0, 0, r, g, b, a },
		{ x, y + h, 0, 0, r, g, b, a },
		{ x, y, 0, 0, r, g, b, a },
	};
	glUseProgram(program);
	glBindVertexArray(vao);
	glUniform1i(loc_useTexture, 0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
	glLineWidth(lineWidth);
	glDrawArrays(GL_LINE_STRIP, 0, 5);
	glBindVertexArray(0);
	glUseProgram(0);
}

void GLRenderer::drawLine(float x1, float y1, float x2, float y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float width) {
	flushBatch();
	Vertex verts[2] = {
		{ x1, y1, 0, 0, r, g, b, a },
		{ x2, y2, 0, 0, r, g, b, a },
	};
	glUseProgram(program);
	glBindVertexArray(vao);
	glUniform1i(loc_useTexture, 0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
	glLineWidth(width);
	glDrawArrays(GL_LINES, 0, 2);
	glBindVertexArray(0);
	glUseProgram(0);
}

void GLRenderer::drawLines(const float* vertices, int pairCount, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float width) {
	flushBatch();
	int count = pairCount * 2;
	std::vector<Vertex> verts(count);
	for (int i = 0; i < count; ++i) {
		verts[i] = { vertices[i * 2], vertices[i * 2 + 1], 0, 0, r, g, b, a };
	}
	glUseProgram(program);
	glBindVertexArray(vao);
	glUniform1i(loc_useTexture, 0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_DYNAMIC_DRAW);
	glLineWidth(width);
	glDrawArrays(GL_LINES, 0, (GLsizei)count);
	glBindVertexArray(0);
	glUseProgram(0);
}

void GLRenderer::drawStippledLines(const float* vertices, int pairCount, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float width, int factor, uint16_t pattern) {
	flushBatch();
	glUseProgram(0);
	glEnable(GL_LINE_STIPPLE);
	glLineStipple(factor, pattern);
	glLineWidth(width);
	glColor4ub(r, g, b, a);
	glBegin(GL_LINES);
	for (int i = 0; i < pairCount * 2; ++i) {
		glVertex2f(vertices[i * 2], vertices[i * 2 + 1]);
	}
	glEnd();
	glDisable(GL_LINE_STIPPLE);
}

void GLRenderer::drawPolygon(const float* vertices, int vertexCount, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	flushBatch();
	glColor4ub(r, g, b, a);
	glBegin(GL_POLYGON);
	for (int i = 0; i < vertexCount; ++i) {
		glVertex2f(vertices[i * 2], vertices[i * 2 + 1]);
	}
	glEnd();
}

void GLRenderer::drawTriangleFan(const float* vertices, int vertexCount, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	flushBatch();
	glColor4ub(r, g, b, a);
	glBegin(GL_TRIANGLE_FAN);
	for (int i = 0; i < vertexCount; ++i) {
		glVertex2f(vertices[i * 2], vertices[i * 2 + 1]);
	}
	glEnd();
}

void GLRenderer::drawText(float x, float y, const std::string &text, uint8_t r, uint8_t g, uint8_t b, uint8_t a, void* font) {
#if defined(__LINUX__) || defined(__WINDOWS__)
	flushBatch();
	glColor4ub(r, g, b, a);
	glRasterPos2f(x, y);
	for (char c : text) {
		glutBitmapCharacter(font, c);
	}
#endif
}

float GLRenderer::getCharWidth(char c, void* font) {
#if defined(__LINUX__) || defined(__WINDOWS__)
	return static_cast<float>(glutBitmapWidth(font, c));
#else
	return 0.f;
#endif
}

void GLRenderer::setRasterPos(float x, float y) {
#if defined(__LINUX__) || defined(__WINDOWS__)
	flushBatch();
	glRasterPos2f(x, y);
#endif
}

void GLRenderer::drawBitmapChar(char c, void* font) {
#if defined(__LINUX__) || defined(__WINDOWS__)
	flushBatch();
	glutBitmapCharacter(font, c);
#endif
}

void GLRenderer::setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	flushBatch();
	glColor4ub(r, g, b, a);
}

void GLRenderer::enableTexture() {
	// no-op: shader handles texture state via uUseTexture
}

void GLRenderer::disableTexture() {
	// no-op: shader handles texture state via uUseTexture
}

void GLRenderer::flush() {
	flushBatch();
}

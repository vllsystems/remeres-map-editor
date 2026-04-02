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
#elif defined(__APPLE__)
	#include <dlfcn.h>
static void* rmeGetGLProc(const char* name) {
	static void* lib = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
	return lib ? dlsym(lib, name) : nullptr;
}
#else
	#include <dlfcn.h>
	#include <GL/glx.h>
static void* rmeGetGLProc(const char* name) {
	void* p = (void*)glXGetProcAddressARB((const GLubyte*)name);
	if (!p) {
		static void* lib = dlopen("libGL.so.1", RTLD_LAZY);
		if (lib) {
			p = dlsym(lib, name);
		}
	}
	return p;
}
#endif

#include <glad/glad.h>
#include "main.h"
#include "gl_renderer.h"
#include <cmath>

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
uniform int uStipple;
out vec4 FragColor;
void main() {
    if (uStipple != 0) {
        float p = gl_FragCoord.x + gl_FragCoord.y;
        if (mod(p, 4.0) < 2.0) discard;
    }
    if (uUseTexture != 0)
        FragColor = texture(uTexture, vUV) * vColor;
    else
        FragColor = vColor;
}
)";

void GLRenderer::initFontAtlas() {
	const int GLYPH_W = 10;
	const int GLYPH_H = 16;
	const int COLS = 16;
	const int ROWS = 6; // ASCII 32..127 = 96 chars
	const int TEX_W = COLS * GLYPH_W; // 160
	const int TEX_H = ROWS * GLYPH_H; // 96

	wxBitmap bmp(TEX_W, TEX_H, 24);
	{
		wxMemoryDC dc(bmp);
		dc.SetBackground(*wxBLACK_BRUSH);
		dc.Clear();

		wxFont font(wxFontInfo(10).Family(wxFONTFAMILY_MODERN).AntiAliased(false));
		dc.SetFont(font);
		dc.SetTextForeground(*wxWHITE);

		for (int i = 0; i < 96; ++i) {
			char c = (char)(32 + i);
			int col = i % COLS;
			int row = i / COLS;
			dc.DrawText(wxString(c), col * GLYPH_W, row * GLYPH_H);
		}
		dc.SelectObject(wxNullBitmap);
	}

	wxImage img = bmp.ConvertToImage();
	std::vector<uint8_t> rgba(TEX_W * TEX_H * 4);
	for (int y = 0; y < TEX_H; ++y) {
		for (int x = 0; x < TEX_W; ++x) {
			int idx = (y * TEX_W + x) * 4;
			uint8_t lum = img.GetRed(x, y);
			rgba[idx + 0] = 255;
			rgba[idx + 1] = 255;
			rgba[idx + 2] = 255;
			rgba[idx + 3] = lum; // branco = opaco, preto = transparente
		}
	}

	glGenTextures(1, &fontAtlas);
	glBindTexture(GL_TEXTURE_2D, fontAtlas);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_W, TEX_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
	glBindTexture(GL_TEXTURE_2D, 0);

	fontGlyphW = GLYPH_W;
	fontGlyphH = GLYPH_H;
	fontAtlasCols = COLS;
	fontAtlasW = TEX_W;
	fontAtlasH = TEX_H;
}

void GLRenderer::init() {
	if (initialized) {
		return;
	}

	if (!gladLoadGLLoader((GLADloadproc)rmeGetGLProc)) {
		wxLogError("GLRenderer::init — gladLoadGLLoader failed");
		return;
	}

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &vertSrc, nullptr);
	glCompileShader(vs);
	{
		GLint ok = 0;
		glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
		if (!ok) {
			char log[512];
			glGetShaderInfoLog(vs, sizeof(log), nullptr, log);
			wxLogError("GLRenderer::init — vertex shader compile error: %s", log);
			glDeleteShader(vs);
			return;
		}
	}

	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &fragSrc, nullptr);
	glCompileShader(fs);
	{
		GLint ok = 0;
		glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
		if (!ok) {
			char log[512];
			glGetShaderInfoLog(fs, sizeof(log), nullptr, log);
			wxLogError("GLRenderer::init — fragment shader compile error: %s", log);
			glDeleteShader(vs);
			glDeleteShader(fs);
			return;
		}
	}

	program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	{
		GLint ok = 0;
		glGetProgramiv(program, GL_LINK_STATUS, &ok);
		if (!ok) {
			char log[512];
			glGetProgramInfoLog(program, sizeof(log), nullptr, log);
			wxLogError("GLRenderer::init — program link error: %s", log);
			glDeleteProgram(program);
			program = 0;
			glDeleteShader(vs);
			glDeleteShader(fs);
			return;
		}
	}

	glDeleteShader(vs);
	glDeleteShader(fs);

	loc_projection = glGetUniformLocation(program, "uProjection");
	loc_useTexture = glGetUniformLocation(program, "uUseTexture");
	loc_texture = glGetUniformLocation(program, "uTexture");
	loc_stipple = glGetUniformLocation(program, "uStipple");

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

	initFontAtlas();

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
	if (fontAtlas) {
		glDeleteTextures(1, &fontAtlas);
		fontAtlas = 0;
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

void GLRenderer::drawThickLineSegment(float x1, float y1, float x2, float y2, float width, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	float dx = x2 - x1;
	float dy = y2 - y1;
	float len = sqrtf(dx * dx + dy * dy);
	if (len < 1e-6f) {
		return;
	}
	float nx = (-dy / len) * (width * 0.5f);
	float ny = (dx / len) * (width * 0.5f);

	if (current_texture != 0 && !batch.empty()) {
		flushBatch();
	}
	current_texture = 0;

	Vertex v0 = { x1 + nx, y1 + ny, 0, 0, r, g, b, a };
	Vertex v1 = { x1 - nx, y1 - ny, 0, 0, r, g, b, a };
	Vertex v2 = { x2 - nx, y2 - ny, 0, 0, r, g, b, a };
	Vertex v3 = { x2 + nx, y2 + ny, 0, 0, r, g, b, a };

	batch.push_back(v0);
	batch.push_back(v1);
	batch.push_back(v2);
	batch.push_back(v0);
	batch.push_back(v2);
	batch.push_back(v3);
}

void GLRenderer::drawRect(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float lineWidth) {
	drawThickLineSegment(x, y, x + w, y, lineWidth, r, g, b, a);
	drawThickLineSegment(x + w, y, x + w, y + h, lineWidth, r, g, b, a);
	drawThickLineSegment(x + w, y + h, x, y + h, lineWidth, r, g, b, a);
	drawThickLineSegment(x, y + h, x, y, lineWidth, r, g, b, a);
}

void GLRenderer::drawLine(float x1, float y1, float x2, float y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float width) {
	drawThickLineSegment(x1, y1, x2, y2, width, r, g, b, a);
}

void GLRenderer::drawLines(const float* vertices, int pairCount, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float width) {
	for (int i = 0; i < pairCount; ++i) {
		float x1 = vertices[i * 4];
		float y1 = vertices[i * 4 + 1];
		float x2 = vertices[i * 4 + 2];
		float y2 = vertices[i * 4 + 3];
		drawThickLineSegment(x1, y1, x2, y2, width, r, g, b, a);
	}
}

void GLRenderer::drawStippledLines(const float* vertices, int pairCount, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float width, int factor, uint16_t pattern) {
	for (int i = 0; i < pairCount; ++i) {
		float x1 = vertices[i * 4];
		float y1 = vertices[i * 4 + 1];
		float x2 = vertices[i * 4 + 2];
		float y2 = vertices[i * 4 + 3];

		float dx = x2 - x1;
		float dy = y2 - y1;
		float len = sqrtf(dx * dx + dy * dy);
		if (len < 1e-6f) {
			continue;
		}

		float dirX = dx / len;
		float dirY = dy / len;
		float step = (float)factor;
		int bit = 0;
		float pos = 0.0f;

		while (pos < len) {
			float segEnd = pos + step;
			if (segEnd > len) {
				segEnd = len;
			}

			if (pattern & (1 << (bit & 15))) {
				float sx = x1 + dirX * pos;
				float sy = y1 + dirY * pos;
				float ex = x1 + dirX * segEnd;
				float ey = y1 + dirY * segEnd;
				drawThickLineSegment(sx, sy, ex, ey, width, r, g, b, a);
			}

			pos = segEnd;
			bit++;
		}
	}
}

void GLRenderer::drawPolygon(const float* vertices, int vertexCount, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	if (vertexCount < 3) {
		return;
	}
	flushBatch();
	std::vector<Vertex> tris;
	for (int i = 1; i < vertexCount - 1; ++i) {
		tris.push_back({ vertices[0], vertices[1], 0, 0, r, g, b, a });
		tris.push_back({ vertices[i * 2], vertices[i * 2 + 1], 0, 0, r, g, b, a });
		tris.push_back({ vertices[(i + 1) * 2], vertices[(i + 1) * 2 + 1], 0, 0, r, g, b, a });
	}
	glUseProgram(program);
	glBindVertexArray(vao);
	glUniform1i(loc_useTexture, 0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, tris.size() * sizeof(Vertex), tris.data(), GL_DYNAMIC_DRAW);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)tris.size());
	glBindVertexArray(0);
	glUseProgram(0);
}

void GLRenderer::drawTriangleFan(const float* vertices, int vertexCount, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	if (vertexCount < 3) {
		return;
	}
	flushBatch();
	std::vector<Vertex> tris;
	for (int i = 1; i < vertexCount - 1; ++i) {
		tris.push_back({ vertices[0], vertices[1], 0, 0, r, g, b, a });
		tris.push_back({ vertices[i * 2], vertices[i * 2 + 1], 0, 0, r, g, b, a });
		tris.push_back({ vertices[(i + 1) * 2], vertices[(i + 1) * 2 + 1], 0, 0, r, g, b, a });
	}
	glUseProgram(program);
	glBindVertexArray(vao);
	glUniform1i(loc_useTexture, 0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, tris.size() * sizeof(Vertex), tris.data(), GL_DYNAMIC_DRAW);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)tris.size());
	glBindVertexArray(0);
	glUseProgram(0);
}

void GLRenderer::drawText(float x, float y, const std::string &text, uint8_t r, uint8_t g, uint8_t b, uint8_t a, void* font) {
	if (fontAtlas == 0) {
		return;
	}
	if (current_texture != fontAtlas) {
		flushBatch();
		current_texture = fontAtlas;
	}
	float cx = x;
	for (char c : text) {
		if (c < 32 || c > 127) {
			continue;
		}
		int idx = c - 32;
		int col = idx % fontAtlasCols;
		int row = idx / fontAtlasCols;
		float u0 = (float)(col * fontGlyphW) / fontAtlasW;
		float v0 = (float)(row * fontGlyphH) / fontAtlasH;
		float u1 = (float)((col + 1) * fontGlyphW) / fontAtlasW;
		float v1 = (float)((row + 1) * fontGlyphH) / fontAtlasH;
		float qx = cx;
		float qy = y - fontGlyphH;
		float qw = (float)fontGlyphW;
		float qh = (float)fontGlyphH;
		batch.push_back({ qx, qy, u0, v0, r, g, b, a });
		batch.push_back({ qx + qw, qy, u1, v0, r, g, b, a });
		batch.push_back({ qx + qw, qy + qh, u1, v1, r, g, b, a });
		batch.push_back({ qx, qy, u0, v0, r, g, b, a });
		batch.push_back({ qx + qw, qy + qh, u1, v1, r, g, b, a });
		batch.push_back({ qx, qy + qh, u0, v1, r, g, b, a });
		cx += fontGlyphW;
	}
}

float GLRenderer::getCharWidth(char c, void* font) {
	return (float)fontGlyphW;
}

void GLRenderer::setRasterPos(float x, float y) {
	cursorX = x;
	cursorY = y;
}

void GLRenderer::drawBitmapChar(char c, void* font) {
	if (fontAtlas == 0 || c < 32 || c > 127) {
		return;
	}
	if (current_texture != fontAtlas) {
		flushBatch();
		current_texture = fontAtlas;
	}
	int idx = c - 32;
	int col = idx % fontAtlasCols;
	int row = idx / fontAtlasCols;
	float u0 = (float)(col * fontGlyphW) / fontAtlasW;
	float v0 = (float)(row * fontGlyphH) / fontAtlasH;
	float u1 = (float)((col + 1) * fontGlyphW) / fontAtlasW;
	float v1 = (float)((row + 1) * fontGlyphH) / fontAtlasH;
	float qx = cursorX;
	float qy = cursorY - fontGlyphH;
	float qw = (float)fontGlyphW;
	float qh = (float)fontGlyphH;
	batch.push_back({ qx, qy, u0, v0, textR, textG, textB, textA });
	batch.push_back({ qx + qw, qy, u1, v0, textR, textG, textB, textA });
	batch.push_back({ qx + qw, qy + qh, u1, v1, textR, textG, textB, textA });
	batch.push_back({ qx, qy, u0, v0, textR, textG, textB, textA });
	batch.push_back({ qx + qw, qy + qh, u1, v1, textR, textG, textB, textA });
	batch.push_back({ qx, qy + qh, u0, v1, textR, textG, textB, textA });
	cursorX += fontGlyphW;
}

void GLRenderer::setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	flushBatch();
	textR = r;
	textG = g;
	textB = b;
	textA = a;
}

void GLRenderer::flush() {
	flushBatch();
}

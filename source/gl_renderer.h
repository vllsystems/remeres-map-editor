#ifndef RME_GL_RENDERER_H_
#define RME_GL_RENDERER_H_

#include <string>
#include <cstdint>
#include <vector>

// Minimal GL type forward declarations — full GL comes from glad in gl_renderer.cpp
typedef unsigned int GLuint;
typedef int GLint;

struct GLColor {
	uint8_t r, g, b, a;
};

class GLRenderer {
public:
	void init();
	void shutdown();

	void drawTexturedQuad(float x, float y, float w, float h, GLuint textureId, const GLColor &color);

	void drawColoredQuad(float x, float y, float w, float h, const GLColor &color);

	void drawRect(float x, float y, float w, float h, const GLColor &color, float lineWidth = 1.0f);

	void drawLine(float x1, float y1, float x2, float y2, const GLColor &color, float width = 1.0f);

	void drawLines(const float* vertices, int pairCount, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float width = 1.0f);

	void drawStippledLines(const float* vertices, int pairCount, const GLColor &color, float width = 1.0f, int factor = 2, uint16_t pattern = 0xAAAA);

	void drawPolygon(const float* vertices, int vertexCount, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

	void drawTriangleFan(const float* vertices, int vertexCount, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

	void drawText(float x, float y, const std::string &text, uint8_t r, uint8_t g, uint8_t b, uint8_t a, void* font = nullptr);
	float getCharWidth(char c, void* font = nullptr);
	void setRasterPos(float x, float y);
	void drawBitmapChar(char c, void* font);
	void setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

	void setOrtho(float left, float right, float bottom, float top);

	void flush();
	static void invalidateTexture(GLuint id);

private:
	static GLRenderer* s_instance;
	bool initialized = false;
	GLuint vao = 0;
	GLuint vbo = 0;
	GLuint program = 0;
	GLint loc_projection = -1;
	GLint loc_useTexture = -1;
	GLint loc_texture = -1;
	GLint loc_stipple = -1;

	struct Vertex {
		float x, y;
		float u, v;
		uint8_t r, g, b, a;
	};

	std::vector<Vertex> batch;
	GLuint current_texture = 0;

	void flushBatch();
	void drawThickLineSegment(float x1, float y1, float x2, float y2, float width, const GLColor &color);

	// Font atlas
	GLuint fontAtlas = 0;
	int fontGlyphW = 0;
	int fontGlyphH = 0;
	int fontAtlasCols = 0;
	int fontAtlasW = 0;
	int fontAtlasH = 0;
	void initFontAtlas();

	// Text cursor state (for setRasterPos + drawBitmapChar)
	float cursorX = 0;
	float cursorY = 0;
	uint8_t textR = 255, textG = 255, textB = 255, textA = 255;
};

#endif

#include "main.h"
#include "gl_renderer.h"

void GLRenderer::init() {
	// noop - Stage 2 will create shaders, VAO, VBO
}

void GLRenderer::shutdown() {
	// noop - Stage 2 will cleanup
}

void GLRenderer::drawTexturedQuad(float x, float y, float w, float h, GLuint textureId, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	glBindTexture(GL_TEXTURE_2D, textureId);
	glColor4ub(r, g, b, a);
	glBegin(GL_QUADS);
	glTexCoord2f(0.f, 0.f);
	glVertex2f(x, y);
	glTexCoord2f(1.f, 0.f);
	glVertex2f(x + w, y);
	glTexCoord2f(1.f, 1.f);
	glVertex2f(x + w, y + h);
	glTexCoord2f(0.f, 1.f);
	glVertex2f(x, y + h);
	glEnd();
}

void GLRenderer::drawColoredQuad(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	glColor4ub(r, g, b, a);
	glBegin(GL_QUADS);
	glVertex2f(x, y);
	glVertex2f(x + w, y);
	glVertex2f(x + w, y + h);
	glVertex2f(x, y + h);
	glEnd();
}

void GLRenderer::drawRect(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float lineWidth) {
	glLineWidth(lineWidth);
	glColor4ub(r, g, b, a);
	glBegin(GL_LINE_STRIP);
	glVertex2f(x, y);
	glVertex2f(x + w, y);
	glVertex2f(x + w, y + h);
	glVertex2f(x, y + h);
	glVertex2f(x, y);
	glEnd();
}

void GLRenderer::drawLine(float x1, float y1, float x2, float y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float width) {
	glLineWidth(width);
	glColor4ub(r, g, b, a);
	glBegin(GL_LINES);
	glVertex2f(x1, y1);
	glVertex2f(x2, y2);
	glEnd();
}

void GLRenderer::drawLines(const float* vertices, int pairCount, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float width) {
	glLineWidth(width);
	glColor4ub(r, g, b, a);
	glBegin(GL_LINES);
	for (int i = 0; i < pairCount * 2; ++i) {
		glVertex2f(vertices[i * 2], vertices[i * 2 + 1]);
	}
	glEnd();
}

void GLRenderer::drawStippledLines(const float* vertices, int pairCount, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float width, int factor, uint16_t pattern) {
	glEnable(GL_LINE_STIPPLE);
	glLineStipple(factor, pattern);
	drawLines(vertices, pairCount, r, g, b, a, width);
	glDisable(GL_LINE_STIPPLE);
}

void GLRenderer::drawPolygon(const float* vertices, int vertexCount, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	glColor4ub(r, g, b, a);
	glBegin(GL_POLYGON);
	for (int i = 0; i < vertexCount; ++i) {
		glVertex2f(vertices[i * 2], vertices[i * 2 + 1]);
	}
	glEnd();
}

void GLRenderer::drawTriangleFan(const float* vertices, int vertexCount, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	glColor4ub(r, g, b, a);
	glBegin(GL_TRIANGLE_FAN);
	for (int i = 0; i < vertexCount; ++i) {
		glVertex2f(vertices[i * 2], vertices[i * 2 + 1]);
	}
	glEnd();
}

void GLRenderer::drawText(float x, float y, const std::string &text, uint8_t r, uint8_t g, uint8_t b, uint8_t a, void* font) {
#if defined(__LINUX__) || defined(__WINDOWS__)
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
	glRasterPos2f(x, y);
#endif
}

void GLRenderer::drawBitmapChar(char c, void* font) {
#if defined(__LINUX__) || defined(__WINDOWS__)
	glutBitmapCharacter(font, c);
#endif
}

void GLRenderer::setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	glColor4ub(r, g, b, a);
}

void GLRenderer::enableTexture() {
	glEnable(GL_TEXTURE_2D);
}

void GLRenderer::disableTexture() {
	glDisable(GL_TEXTURE_2D);
}

void GLRenderer::flush() {
	// noop - Stage 2 will flush batched geometry
}

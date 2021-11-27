#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <cmath>
#include <map>

namespace bvg {

enum class LineJoin {
    Bevel,
    Round,
    Miter
};

enum class LineCap {
    Butt,
    Round,
    Square
};

struct Color {
    Color();
    Color(float r, float g, float b, float a = 1.0f);
    float r, g, b, a;
    
    static Color lerp(Color a, Color b, float t);
};

namespace colors {

static const Color Black = Color(0.0f, 0.0f, 0.0f);
static const Color White = Color(1.0f, 1.0f, 1.0f);

} // namespace colors

struct Style {
    Style();
    
    enum class Type {
        SolidColor,
        LinearGradient,
        RadialGradient,
        ConicGradient
    };
    struct Linear {
        Color startColor;
        float startX = 0.0f;
        float startY = 0.0f;
        Color endColor;
        float endX = 0.0f;
        float endY = 0.0f;
    };
    struct Radial {
        Color startColor;
        Color endColor;
        float x;
        float y;
        float radius;
    };
    struct Conic {
        Color startColor;
        Color endColor;
        float x;
        float y;
        float angle;
    };
    
    Type type;
    union {
        Color color;
        Linear linear;
        Radial radial;
        Conic conic;
    };
};

Style SolidColor(Color color);
Style LinearGradient(float sx, float sy, float ex, float ey, Color start, Color end);
Style RadialGradient(float x, float y, float radius, Color start, Color end);
Style ConicGradient(float x, float y, float angle, Color start, Color end);

struct LineDash {
    LineDash();
    LineDash(float length, float gapLength, float offset = 0.0f);
    float length, gapLength, offset;
};

enum class BlendingMode {
    Normal,
    Add,
    Subtract,
    Multiply,
    Divide,
    Screen,
    Difference
};

namespace factory {

struct TwoPolylines {
    std::vector<glm::vec2> first, second;
};

struct TriangeIndices {
    int a, b, c;
};

struct ShapeMesh {
    std::vector<glm::vec2> vertices;
    std::vector<TriangeIndices> indices;
    
    void add(ShapeMesh& b);
};

ShapeMesh strokePolyline(std::vector<glm::vec2>& points, const float diameter);
ShapeMesh bevelJoin(std::vector<glm::vec2>& a, std::vector<glm::vec2>& b, const float diameter);
ShapeMesh roundJoin(std::vector<glm::vec2>& a, std::vector<glm::vec2>& b, const float diameter);
ShapeMesh miterJoin(std::vector<glm::vec2>& a, std::vector<glm::vec2>& b, const float diameter,
                    float miterLimitAngle = M_PI_2 + M_PI_4);

TwoPolylines dividePolyline(std::vector<glm::vec2>& points, float t);

glm::vec2 getPointAtT(std::vector<glm::vec2>& points, float t);

std::vector<std::vector<glm::vec2>> dashedPolyline(std::vector<glm::vec2>& points,
                                                   float dashLength, float gapLength,
                                                   float offset = 0.0f);

ShapeMesh roundedCap(glm::vec2 position, glm::vec2 direction, const float diameter);
ShapeMesh squareCap(glm::vec2 position, glm::vec2 direction, const float diameter);

std::vector<float> measurePolyline(std::vector<glm::vec2>& points);
float lengthOfPolyline(std::vector<glm::vec2>& points);
float tAtLength(float length, std::vector<float>& lengths);

std::vector<TriangeIndices> createIndicesConvex(int numVertices);

} // namespace factory

namespace math {

glm::mat4 toMatrix3D(glm::mat3 mat2d);
bool isPointInTriange(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 point);

} // namespace math

class Font {
public:
    struct Atlas {
        int width, height;
    };
    
    struct Bounds {
        float top, left, right, bottom;
    };
    
    struct Character {
        int unicode, advance;
        Bounds planeBounds, atlasBounds;
    };
    
    Atlas atlas;
    int size, lineHeight, baseline;
    int distanceRange;
    
protected:
    virtual void loadCharacter(Character& character);
    void parseJson(std::string& json);
};

class Context {
public:
    Context(float width, float height);
    Context();
    
    float width = 0;
    float height = 0;
    float contentScale = 1.0f;
    
    glm::mat4 viewProj = glm::mat4(1.0f);
    glm::mat3 matrix = glm::mat3(1.0f);
    
    LineJoin lineJoin = LineJoin::Miter;
    LineCap lineCap = LineCap::Butt;
    LineDash lineDash = LineDash();
    float lineWidth = 2.0f;
    
    Style fillStyle = SolidColor(colors::Black);
    Style strokeStyle = SolidColor(colors::Black);
    
    int blurRadius = 0;
    
    std::map<std::string, Font*> fonts;
    Font* font = nullptr;
    float fontSize = 32.0f;
    
    void loadFont(std::string jsonPath, std::string imagePath, std::string fontName);
    virtual void loadFontFromMemory(std::string& json,
                                    std::string fontName,
                                    void* imageData,
                                    int width,
                                    int height,
                                    int numChannels);
    
    void orthographic(float width, float height);
    
    void translate(float x, float y);
    void scale(float x, float y);
    void shearX(float x);
    void shearY(float y);
    void rotate(float a);
    void clearTransform();
    
    void beginPath();
    void closePath();
    
    void beginClip();
    void endClip();
    
    virtual void convexFill();
    virtual void fill();
    virtual void stroke();
    
    virtual void textFill(std::wstring str, float x, float y);
    virtual void textFillOnPath(std::wstring str, float x = 0, float y = 0);
    
    virtual float measureTextWidth(std::wstring str);
    virtual float measureTextHeight();
    
    void moveTo(float x, float y);
    void lineTo(float x, float y);
    void cubicTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y);
    void quadraticTo(float cpx, float cpy, float x, float y);
    
    void arc(float x, float y, float radius, float startAngle, float endAngle);
    void rect(float x, float y, float width, float height, float radius);
    void rect(float x, float y, float width, float height);
    void rect(float x, float y, float width, float height,
              float topLeftRadius, float topRightRadius,
              float bottomRightRadius, float bottomLeftRadius);
    
    virtual ~Context();
    
protected:
    std::vector<std::vector<glm::vec2>> mPolylines;
    bool mIsPolylineClosed = false;
    glm::vec2 mCurrentPos;
    
    std::vector<glm::vec2> toOnePolyline(std::vector<std::vector<glm::vec2>> polylines);
    
    factory::ShapeMesh internalFill();
    factory::ShapeMesh internalConvexFill();
    factory::ShapeMesh internalStroke();
    
    std::vector<factory::TriangeIndices> debugTriangulate(std::vector<glm::vec2>& vertices,
                                                          bool draw);
};

namespace earcut {

std::vector<factory::TriangeIndices> triangulate(std::vector<glm::vec2>& vertices);

} // namespace earcut

} // namespace bvg

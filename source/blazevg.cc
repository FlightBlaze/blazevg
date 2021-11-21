#include <blazevg.hh>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/vector_angle.hpp>

namespace bvg {

namespace factory {

ShapeMesh strokePolyline(std::vector<glm::vec2>& points, const float diameter) {
    float radius = diameter / 2.0f;
    size_t numPoints = points.size();
    ShapeMesh mesh;
    
    if(points.size() < 2)
        return mesh;
    
    // Calculate arrays size
    mesh.vertices.resize(numPoints * 2);
    mesh.indices.resize(numPoints * 2 - 2);
    
    // For each point we add two points to the mesh and
    // connect them with previous two points if any
    for(size_t i = 0; i < numPoints; i++) {
        // Check for tips
        bool isStart = i == 0;
        bool isEnd = i == numPoints - 1;

        // Retrieve points
        glm::vec2 currentPoint = points[i];
        glm::vec2 backPoint = !isStart ? points[i - 1LL] : currentPoint;
        glm::vec2 nextPoint = !isEnd   ? points[i + 1LL] : currentPoint;
        
        // Calculate three diections
        glm::vec2 forwardDir  = glm::normalize(nextPoint - currentPoint);
        glm::vec2 backwardDir = glm::normalize(backPoint - currentPoint) * -1.0f;
        glm::vec2 meanDir;
        
        // Calculate mean of forward and backward directions
        if (isStart) meanDir = forwardDir;
        else if (isEnd) meanDir = backwardDir;
        else meanDir = (forwardDir + backwardDir) / 2.0f;
        
        // Perpendicular to mean direction
        glm::vec2 a = glm::vec2(meanDir.y, -meanDir.x) * radius;
        glm::vec2 b = -a;
        
        // Put put vectors into the right place
        a += currentPoint;
        b += currentPoint;
        
        // Add vectors to mesh vertices
        size_t currentVertexIndex = i * 2;
        size_t previousVertexIndex = currentVertexIndex - 2;
        mesh.vertices.at(currentVertexIndex) = a;
        mesh.vertices.at(currentVertexIndex + 1LL) = b;
        
        // Connect vertices with bridge of two trianges
        if(!isStart) {
            int idxA = currentVertexIndex;
            int idxB = currentVertexIndex + 1;
            int idxC = previousVertexIndex;
            int idxD = previousVertexIndex + 1;
            TriangeIndices m { idxA, idxB, idxC };
            TriangeIndices k { idxB, idxC, idxD };
            size_t index = i * 2LL - 2;
            mesh.indices.at(index) = m;
            mesh.indices.at(index + 1LL) = k;
        }
    }
    return mesh;
}

void ShapeMesh::add(ShapeMesh& b) {
    int plusVertices = this->vertices.size();
    int plusIndices = this->indices.size();
    this->vertices.resize(this->vertices.size() + b.vertices.size());
    this->indices.resize(this->indices.size() + b.indices.size());
    for(int i = 0; i < b.vertices.size(); i++)
        this->vertices[i + plusVertices] = b.vertices[i];
    for(int i = 0; i < b.indices.size(); i++) {
        TriangeIndices tri = b.indices[i];
        tri.a += plusVertices;
        tri.b += plusVertices;
        tri.c += plusVertices;
        this->indices[i + plusIndices] = tri;
    }
}

bool isCurvesCorrectForJoining(std::vector<glm::vec2>& a, std::vector<glm::vec2>& b) {
    if (a.size() < 2 || b.size() < 2)
        return false;
    return true;
}

ShapeMesh bevelJoin(std::vector<glm::vec2>& a, std::vector<glm::vec2>& b, const float diameter) {
    float radius = diameter / 2.0f;
    ShapeMesh mesh;
    
    if(!isCurvesCorrectForJoining(a, b))
        return mesh;
    
    glm::vec2 center = b.at(0);
    glm::vec2 dirA = glm::normalize(a.at(a.size() - 1) - a.at(a.size() - 2));
    glm::vec2 dirB = glm::normalize(b.at(1) - b.at(0));
    
    // --A   C-__
    //   |  /    --
    // --B  D-__
    //          --
    
    glm::vec2 Ad = glm::vec2(dirA.y, -dirA.x) * radius;
    glm::vec2 Bd = -Ad;
    glm::vec2 Cd = glm::vec2(dirB.y, -dirB.x) * radius;
    glm::vec2 Dd = -Cd;
    glm::vec2 A = Ad + center;
    glm::vec2 B = Bd + center;
    glm::vec2 C = Cd + center;
    glm::vec2 D = Dd + center;
    
    float angle = glm::orientedAngle(dirA, dirB);
    
    mesh.vertices.resize(3);
    mesh.vertices.at(0) = center;
    if(angle > 0) {
        mesh.vertices.at(1) = A;
        mesh.vertices.at(2) = C;
    }
    else {
        mesh.vertices.at(1) = B;
        mesh.vertices.at(2) = D;
    }
    TriangeIndices tri { 0, 1, 2 };
    mesh.indices.push_back(tri);
    
    return mesh;
}

std::vector<glm::vec2> quadraticBezier(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, int segments) {
    auto points = std::vector<glm::vec2>(segments);
    float step = 1.0f / (segments - 1);
    float t = 0.0f;
    for (int i = 0; i < segments; i++) {
        glm::vec2 q0 = glm::mix(p0, p1, t);
        glm::vec2 q1 = glm::mix(p1, p2, t);
        glm::vec2 r = glm::mix(q0, q1, t);
        points[i] = r;
        t += step;
    }
    return points;
}

std::vector<glm::vec2> cubicBezier(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, int segments) {
    auto points = std::vector<glm::vec2>(segments);
    float step = 1.0f / (segments - 1);
    float t = 0.0f;
    for (int i = 0; i < segments; i++) {
        glm::vec2 q0 = glm::mix(p0, p1, t);
        glm::vec2 q1 = glm::mix(p1, p2, t);
        glm::vec2 q2 = glm::mix(p2, p3, t);
        glm::vec2 r0 = glm::mix(q0, q1, t);
        glm::vec2 r1 = glm::mix(q1, q2, t);
        glm::vec2 b = glm::mix(r0, r1, t);
        points[i] = b;
        t += step;
    }
    return points;
}

std::vector<TriangeIndices> createIndicesConvex(int numVertices) {
    size_t amount = numVertices - 2;
    auto indices = std::vector<TriangeIndices>(amount);
    int k = 1;
    for (size_t i = 0; i < amount; i++)
    {
        indices[i].a = 0;
        
        // Connect current edge with next
        indices[i].b = k;
        indices[i].c = k + 1;
        k++;
    }
    return indices;
}

std::vector<glm::vec2> createArc(float startAngle, float endAngle, float radius, int segments, glm::vec2 offset) {
    auto arcVerts = std::vector<glm::vec2>(segments);
    float angle = startAngle;
    float arcLength = endAngle - startAngle;
    for (int i = 0; i <= segments - 1; i++) {
        float x = sin(angle) * radius;
        float y = cos(angle) * radius;

        arcVerts[i] = glm::vec2(x, y) + offset;
        angle += (arcLength / segments);
    }
    return arcVerts;
}

float positiveAngle(float angle) {
    if(angle < 0) {
        return M_PI * 2 + angle;
    }
    return angle;
}

ShapeMesh roundJoin(std::vector<glm::vec2>& a, std::vector<glm::vec2>& b, const float diameter) {
    float radius = diameter / 2.0f;
    ShapeMesh mesh;
    
    if(!isCurvesCorrectForJoining(a, b))
        return mesh;
    
    glm::vec2 center = b.at(0);
    glm::vec2 dirA = glm::normalize(a.at(a.size() - 1) - a.at(a.size() - 2));
    glm::vec2 dirB = glm::normalize(b.at(1) - b.at(0));
    
    glm::vec2 Ad = glm::vec2(dirA.y, -dirA.x) * radius;
    glm::vec2 Bd = -Ad;
    glm::vec2 Cd = glm::vec2(dirB.y, -dirB.x) * radius;
    glm::vec2 Dd = -Cd;
    glm::vec2 A = Ad + center;
    glm::vec2 B = Bd + center;
    glm::vec2 C = Cd + center;
    glm::vec2 D = Dd + center;
    
    float angle = glm::orientedAngle(dirA, dirB);
    
    static const int numCurveSegments = 32;
    mesh.vertices.resize(1);
    mesh.vertices.at(0) = center;
    std::vector<glm::vec2> curve;
    float start = 0;
    float end = 0;
    
    glm::vec2 up;
    
    if(angle > 0) {
        start = atan2(Ad.x, Ad.y);
        end = atan2(Cd.x, Cd.y) - 0.1f;
        up = Ad;
    }
    else {
        start = atan2f(Bd.x, Bd.y);
        end = atan2f(Dd.x, Dd.y) + 0.1f;
        up = Bd;
    }
    curve = createArc(start, end, radius, 32, center);
    
    // if arc is inverted
    if(glm::dot(glm::normalize(curve.at(15) - center), up) < 0) {
        start = positiveAngle(start);
        end = positiveAngle(end);
        curve = createArc(start, end, radius, 32, center);
    }
    mesh.vertices.insert(mesh.vertices.end(), curve.begin(), curve.end());
    std::vector<TriangeIndices> tris = createIndicesConvex(mesh.vertices.size());
    mesh.indices.insert(mesh.indices.end(), tris.begin(), tris.end());
    
    return mesh;
}

glm::vec2 EliminationLineLineIntersection(glm::vec2 v1, glm::vec2 v2, glm::vec2 v3, glm::vec2 v4)
{
    float x12 = v1.x - v2.x;
    float x34 = v3.x - v4.x;
    float y12 = v1.y - v2.y;
    float y34 = v3.y - v4.y;
    float c = x12 * y34 - y12 * x34;
    float a = v1.x * v2.y - v1.y * v2.x;
    float b = v3.x * v4.y - v3.y * v4.x;
    if(c != 0) {
        float x = (a * x34 - b * x12) / c;
        float y = (a * y34 - b * y12) / c;
        return glm::vec2(x, y);
    }
    else {
        // Lines are parallel
        return glm::vec2(0.0f);
    }
}

glm::vec2 LineLineIntersection(glm::vec2 v1, glm::vec2 v2, glm::vec2 v3, glm::vec2 v4) {
    return EliminationLineLineIntersection(v1, v2, v3, v4);
}

ShapeMesh miterJoin(std::vector<glm::vec2>& a, std::vector<glm::vec2>& b, const float diameter,
                    float miterLimitAngle) {
    float radius = diameter / 2.0f;
    ShapeMesh mesh;
    
    if(!isCurvesCorrectForJoining(a, b))
        return mesh;
    
    glm::vec2 center = b.at(0);
    glm::vec2 dirA = glm::normalize(a.at(a.size() - 1) - a.at(a.size() - 2));
    glm::vec2 dirB = glm::normalize(b.at(1) - b.at(0));
    
    float angle = glm::orientedAngle(dirA, dirB);
    
    if (fabsf(angle) > miterLimitAngle)
        return bevelJoin(a, b, diameter);
    
    glm::vec2 Ad = glm::vec2(dirA.y, -dirA.x) * radius;
    glm::vec2 Bd = -Ad;
    glm::vec2 Cd = glm::vec2(dirB.y, -dirB.x) * radius;
    glm::vec2 Dd = -Cd;
    glm::vec2 A = Ad + center;
    glm::vec2 B = Bd + center;
    glm::vec2 C = Cd + center;
    glm::vec2 D = Dd + center;
    
    mesh.vertices.resize(4);
    mesh.vertices.at(0) = center;
    if(angle > 0) {
        glm::vec2 I = LineLineIntersection(A, A + dirA, C, C + dirB);
        mesh.vertices.at(1) = A;
        mesh.vertices.at(2) = C;
        mesh.vertices.at(3) = I;
    }
    else {
        glm::vec2 I = LineLineIntersection(B, B + dirA, D, D + dirB);
        mesh.vertices.at(1) = B;
        mesh.vertices.at(2) = D;
        mesh.vertices.at(3) = I;
    }
    //   0
    //  / \
    // 1---2
    //  \ /
    //   3
    std::vector<TriangeIndices> tris = {
        { 0, 1, 2 },
        { 1, 2, 3 }
    };
    mesh.indices.insert(mesh.indices.end(), tris.begin(), tris.end());
    
    return mesh;
}

TwoPolylines dividePolyline(std::vector<glm::vec2>& points, float t) {
    TwoPolylines twoLines;
    if (t <= 0) {
        twoLines.second = points;
        return twoLines;
    }
    if (t >= 1) {
        twoLines.first = points;
        return twoLines;
    }
    float remapedT = t * (float)(points.size() - 1);
    int segmentIdx = (int)floorf(remapedT);
    float segmentT = remapedT - (float)segmentIdx;
    
    twoLines.first.resize(segmentIdx + 2);
    for (int i = 0; i < segmentIdx + 1; i++) {
        twoLines.first.at(i) = points.at(i);
    }
    twoLines.first.at(segmentIdx + 1) = glm::mix(points.at(segmentIdx), points.at(segmentIdx + 1), segmentT);
    
    twoLines.second.resize(points.size() - segmentIdx);
    for (int i = 1; i < twoLines.second.size(); i++){
        twoLines.second.at(i) = points.at(segmentIdx + i);
    }
    twoLines.second.at(0) = glm::mix(points.at(segmentIdx), points.at(segmentIdx + 1), segmentT);
    
    return twoLines;
}

float lengthOfPolyline(std::vector<glm::vec2>& points) {
    if (points.size() < 2)
        return 0.0f;

    glm::vec2 previousPoint = points.at(0);
    float length = 0.0f;
    for (size_t i = 1; i < points.size(); i++) {
        length += glm::distance(points.at(i), previousPoint);
        previousPoint = points.at(i);
    }
    return length;
}

std::vector<float> measurePolyline(std::vector<glm::vec2>& points) {
    std::vector<float> lengths;
    lengths.resize(points.size() - 1); // number of segments
    for (size_t i = 0; i < lengths.size(); i++) {
        lengths.at(i) = glm::distance(points.at(i), points.at(i + 1LL));
    }
    return lengths;
}

float tAtLength(float length, std::vector<float>& lengths) {
    size_t pointBeforeLength = 0;
    float previousLength = 0.0f;
    float currentLength = 0.0f;
    float localT = 0.0f;
    if(lengths.size() > 0) {
        localT = fminf(length, lengths.at(0));
    }
    for (size_t i = 0; i < lengths.size(); i++) {
        previousLength = currentLength;
        currentLength += lengths.at(i);
        if (length < currentLength) {
            pointBeforeLength = i;
            localT = (length - previousLength) / lengths.at(i);
            break;
        }
    }
    float t = ((float)pointBeforeLength + localT) / (float)lengths.size();
    return t;
}

std::vector<std::vector<glm::vec2>> dashedPolyline(std::vector<glm::vec2>& points,
                                                   float dashLength, float gapLength,
                                                   float offset) {
    std::vector<std::vector<glm::vec2>> lines;
    std::vector<glm::vec2> currentPath = points;
    
    float dashGapLength = dashLength + gapLength;
    float offsetTimes = floorf(fabsf(offset) / dashGapLength);
    float localOffset = fabsf(offset) - offsetTimes * dashGapLength;
    if(offset > 0) {
        std::vector<float> lengths = measurePolyline(currentPath);
        if(localOffset > gapLength) {
            float startDashLength = localOffset - gapLength;
            lines.push_back(dividePolyline(currentPath, tAtLength(startDashLength, lengths)).first);
        }
        currentPath = dividePolyline(currentPath, tAtLength(localOffset, lengths)).second;
    }
    else if(offset < 0) {
        std::vector<float> lengths = measurePolyline(currentPath);
        if(localOffset < dashLength) {
            float startDashLength = dashLength - localOffset;
            lines.push_back(dividePolyline(currentPath, tAtLength(startDashLength, lengths)).first);
        }
        currentPath = dividePolyline(currentPath, tAtLength(dashGapLength - localOffset,
                                                            lengths)).second;
    }
    
    static const int maxDashes = 999;
    for(int i = 0; i < maxDashes; i++) {
        std::vector<float> lengths = measurePolyline(currentPath);
        TwoPolylines twoPathes = dividePolyline(currentPath, tAtLength(dashLength, lengths));
        if(twoPathes.first.size() < 2)
            break;
        lines.push_back(twoPathes.first);
        if(twoPathes.second.size() < 2)
            break;
        std::vector<float> secondLengths = measurePolyline(twoPathes.second);
        currentPath = dividePolyline(twoPathes.second, tAtLength(gapLength, secondLengths)).second;
        if(currentPath.size() < 2)
            break;
    }
    return lines;
}

ShapeMesh roundedCap(glm::vec2 position, glm::vec2 direction, const float diameter) {
    float radius = diameter / 2.0f;
    ShapeMesh mesh;
    
    glm::vec2 Ad = glm::vec2(direction.y, -direction.x) * radius;
    glm::vec2 Bd = -Ad;
    
    static const int numCurveSegments = 32;
    mesh.vertices.resize(1);
    mesh.vertices.at(0) = position;
    
    float dirAngle = atan2f(direction.x, direction.y);
    float start = glm::orientedAngle(direction, Ad) + dirAngle;
    float end = glm::orientedAngle(direction, Bd) + dirAngle + 0.1f;
    
    std::vector<glm::vec2> curve = createArc(start, end, radius, 32, position);
    mesh.vertices.insert(mesh.vertices.end(), curve.begin(), curve.end());
    
    std::vector<TriangeIndices> tris = createIndicesConvex(mesh.vertices.size());
    mesh.indices.insert(mesh.indices.end(), tris.begin(), tris.end());
    
    return mesh;
}

ShapeMesh squareCap(glm::vec2 position, glm::vec2 direction, const float diameter) {
    std::vector<glm::vec2> points {
        position,
        position + glm::normalize(direction) * diameter
    };
    return strokePolyline(points, diameter);
}

} // namespace factory

Color::Color():
    r(0.0f), g(0.0f), b(0.0f), a(1.0f)
{
}

Color::Color(float r, float g, float b, float a):
    r(r), g(g), b(b), a(a)
{
}

LineDash::LineDash()
    : length(10.0f), gapLength(0.0f), offset(0.0f)
{
}
LineDash::LineDash(float length, float gapLength, float offset)
: length(length), gapLength(gapLength), offset(offset)
{
}

Style SolidColor(Color color) {
    Style style;
    style.type = Style::Type::SolidColor;
    style.color = color;
    return style;
}

Style LinearGradient(float sx, float sy, float ex, float ey, Color start, Color end) {
    Style style;
    style.type = Style::Type::LinearGradient;
    style.gradientStartColor = start;
    style.gradientStartX = sx;
    style.gradientStartY = sy;
    style.gradientEndColor = end;
    style.gradientEndX = ex;
    style.gradientEndY = ey;
    return style;
}

Context::Context(float width, float height) {
    this->orthographic(width, height);
}

Context::Context()
{
}

void Context::orthographic(float width, float height) {
    this->MVP = glm::ortho(0.0f, width, height, 0.0f, -1000.0f, 1000.0f);
    this->width = width;
    this->height = height;
}

float round3f(float t) {
    return roundf(t * 1000.0f) / 1000.0f;
}

bool isApproxEqualVec2(glm::vec2 a, glm::vec2 b) {
    float a1 = round3f(a.x);
    float a2 = round3f(a.y);
    float b1 = round3f(b.x);
    float b2 = round3f(b.y);
    return a1 == b1 && a2 == b2;
}

void Context::beginPath() {
    this->mPolylines.clear();
    this->mIsPolylineClosed = false;
    this->mCurrentPos = glm::vec2(0.0f);
}

void Context::closePath() {
    if(this->mIsPolylineClosed || this->mPolylines.size() < 2)
        return;
    
    this->mIsPolylineClosed = true;
    
    if(isApproxEqualVec2(this->mPolylines.back().back(),
       this->mPolylines.front().front()))
        return;
    
    std::vector<glm::vec2> line {
        this->mPolylines.back().back(),
        this->mPolylines.front().front()
    };
    
    this->mPolylines.push_back(line);
}

void Context::moveTo(float x, float y) {
    mCurrentPos = glm::vec2(x, y);
}

void Context::lineTo(float x, float y) {
    std::vector<glm::vec2> line {
        mCurrentPos,
        glm::vec2(x, y)
    };
    mPolylines.push_back(line);
    mCurrentPos = glm::vec2(x, y);
}

void Context::cubicTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y) {
    std::vector<glm::vec2> curve = factory::cubicBezier(mCurrentPos,
                                                        glm::vec2(cp1x, cp1y),
                                                        glm::vec2(cp2x, cp2y),
                                                        glm::vec2(x, y),
                                                        32);
    mPolylines.push_back(curve);
    mCurrentPos = glm::vec2(x, y);
}

void Context::quadraticTo(float cpx, float cpy, float x, float y) {
    std::vector<glm::vec2> curve = factory::quadraticBezier(mCurrentPos,
                                                        glm::vec2(cpx, cpy),
                                                        glm::vec2(x, y),
                                                        32);
    mPolylines.push_back(curve);
    mCurrentPos = glm::vec2(x, y);
}

std::vector<glm::vec2> Context::toOnePolyline(std::vector<std::vector<glm::vec2>> polylines) {
    std::vector<glm::vec2> onePolyline;
    if(polylines.size() == 0)
        return onePolyline;
    onePolyline = polylines.front();
    for(int i = 1; i < polylines.size(); i++) {
        std::vector<glm::vec2> ongoing = polylines.at(i);
        
        // Remove previous point (duplicate)
        if(isApproxEqualVec2(onePolyline.back(), ongoing.front()))
            ongoing.erase(ongoing.begin());
        
        onePolyline.insert(onePolyline.begin(), ongoing.begin(), ongoing.end());
    }
    return onePolyline;
}

factory::ShapeMesh Context::internalConvexFill() {
    factory::ShapeMesh mesh;
    mesh.vertices = this->toOnePolyline(mPolylines);
    mesh.indices = factory::createIndicesConvex(mesh.vertices.size());
    return mesh;
}

factory::ShapeMesh Context::internalStroke() {
    factory::ShapeMesh mesh;
    auto* allPolylines = &this->mPolylines;
    
    bool isLineDash = this->lineDash.gapLength != 0.0f;
    if(isLineDash) {
        allPolylines = new std::vector<std::vector<glm::vec2>>();
        
        float gapLength = this->lineDash.gapLength;
        // Add extra space for line caps between
        // two polylines
        if(this->lineCap != LineCap::Butt)
            gapLength += this->lineWidth;
        
        for(int i = 0; i < this->mPolylines.size(); i++) {
            std::vector<std::vector<glm::vec2>> dashed =
                factory::dashedPolyline(this->mPolylines[i],
                                        this->lineDash.length,
                                        gapLength,
                                        this->lineDash.offset);
            allPolylines->insert(allPolylines->end(), dashed.begin(), dashed.end());
        }
        // Don't forget to free memory at the end
        // of this function
    }
    
    bool isConnectedWithPrevious = false;
    for(int i = 0; i < allPolylines->size(); i++) {
        bool isFirst = i == 0;
        bool isLast = i == allPolylines->size() - 1;
        
        bool addStartCap = false;
        bool addEndCap = false;
        if(!isConnectedWithPrevious)
            addStartCap = true;
        
        std::vector<glm::vec2>& polyline = allPolylines->at(i);
        factory::ShapeMesh polylineMesh = factory::strokePolyline(polyline, this->lineWidth);
        mesh.add(polylineMesh);
        if(!isLast || mIsPolylineClosed) {
            std::vector<glm::vec2>* nextOrFirstPolyline;
            if(mIsPolylineClosed && isLast)
                nextOrFirstPolyline = &allPolylines->at(0);
            else
                nextOrFirstPolyline = &allPolylines->at(i + 1);
            std::vector<glm::vec2>& nextPolyline = *nextOrFirstPolyline;
            // If next polyline is connected with current.
            // When we using bezier curves, the end tip coords
            // may vary in severay digits after floating point,
            // so we need to round it before comparing
            if(isApproxEqualVec2(polyline.back(), nextPolyline.front())) {
                // Tell next polyline that he is connected
                // with current
                isConnectedWithPrevious = true;
                
                switch (this->lineJoin) {
                    case LineJoin::Miter:
                    {
                        factory::ShapeMesh joinMesh = factory::miterJoin(polyline,
                                                                         nextPolyline,
                                                                         this->lineWidth);
                        mesh.add(joinMesh);
                    }
                        break;
                    case LineJoin::Round:
                    {
                        factory::ShapeMesh joinMesh = factory::roundJoin(polyline,
                                                                         nextPolyline,
                                                                         this->lineWidth);
                        mesh.add(joinMesh);
                    }
                        break;
                    case LineJoin::Bevel:
                    {
                        factory::ShapeMesh joinMesh = factory::bevelJoin(polyline,
                                                                         nextPolyline,
                                                                         this->lineWidth);
                        mesh.add(joinMesh);
                    }
                        break;
                    default:
                        break;
                }
            } else {
                // Tell next polyline that he isn't
                // connected with current
                isConnectedWithPrevious = false;
                addEndCap = true;
            }
        }
        if(isLast) {
            addEndCap = true;
        }
        
        if(mIsPolylineClosed) {
            if(isFirst)
                addStartCap = false;
            if(isLast)
                addEndCap = false;
        }
        
        if(addStartCap) {
            glm::vec2 pos = polyline.front();
            
            // Opposide polyline first segment direction
            glm::vec2 dir = polyline.at(0) - polyline.at(1);
            
            switch (this->lineCap) {
                case LineCap::Round:
                {
                    factory::ShapeMesh capMesh = factory::roundedCap(pos, dir,
                                                                     this->lineWidth);
                    mesh.add(capMesh);
                }
                    break;
                case LineCap::Square:
                {
                    factory::ShapeMesh capMesh = factory::squareCap(pos, dir,
                                                                     this->lineWidth);
                    mesh.add(capMesh);
                }
                    break;
                default:
                    break;
            }
        }
        if(addEndCap) {
            glm::vec2 pos = polyline.back();
            glm::vec2 dir = polyline.at(polyline.size() - 1) - polyline.at(polyline.size() - 2);
            
            switch (this->lineCap) {
                case LineCap::Round:
                {
                    factory::ShapeMesh capMesh = factory::roundedCap(pos, dir,
                                                                     this->lineWidth);
                    mesh.add(capMesh);
                }
                    break;
                case LineCap::Square:
                {
                    factory::ShapeMesh capMesh = factory::squareCap(pos, dir,
                                                                     this->lineWidth);
                    mesh.add(capMesh);
                }
                    break;
                default:
                    break;
            }
        }
    }
    if(isLineDash) {
        delete allPolylines;
    }
    return mesh;
}

void Context::convexFill() {
    
}

void Context::fill() {
    
}

void Context::stroke() {
    
}

void Context::textFill(std::wstring str, float x, float y) {
    
}

void Context::textFillOnPath(std::wstring str) {
    
}

} // namespace bvg

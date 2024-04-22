## 5.3 컴퓨터 색상의 기본 개념

128비트 색상: XMVECTOR를 그대로 활용

32비트 색상: XMCOLOR 클래스(8비트 정수 4개: r, g, b, a)

XMCOLOR->XMVECTOR: XMLoadColor

XMVECTOR->XMCOLOR: XMStoreColor

## 5.5 입력 조립기 단계

IA(input assembler, 입력 조립기): 도형을 조립

DirectX의 정점: 공간적 위치 이외에도 법선 벡터 등 여러 가지 정보를 추가할 수 있다.

정점 버퍼: 정점들을 연속적인 메모리에 저장하여 파이프라인에 묶임

Primitive Topology: 정점 버퍼에 들어있는 정점들의 조합 방식(이 책에서는 일반적으로 삼각형 목록을 사용한다. 삼각형 목록은 3n개의 정점으로 n개의 삼각형을 생성)

정점들이 중복되는 것은 비효율적, 따라서 정점 목록을 만든 다음 이에 해당하는 인덱스를 지정하는 방식으로 정점을 한 번만 사용할 수 있음

(예) 사각형을 만든다고 한다면 기존에는 {v0, v1, v2, v0, v2, v3} 형태로 만들어서 정점이 중복되었다면 {v0, v1, v2, v3}과 {0, 1, 2, 0, 2, 3}으로 만들어 정점을 한 번만 사용(물론 이 경우에는 삼각형 띠를 활용할 수 있다.)

## 5.6 정점 셰이더 단계

VS(Vertex Shader, 정점 셰이더): 정점을 하나 받아서 정점 하나를 출력하는 함수라고 생각할 수 있음

변환, 조명, 변위 매핑 등 다양한 효과를 수행할 수 있다.

World space와 Local space: World space는 전역 좌표계, Local space는 국소 좌표계이다. Local space를 사용하는 이유는 특정한 기하 구조에 최적화된 좌표계를 사용하는 것이 편리하기 때문

Local space 기준 좌표에서 World space 기준 좌표로 변환하는 것을 world matrix라고 한다. World matrix는 scale, rotation, transition(W=SRT)을 조합하여 구성

View space(시야 공간): 카메라의 좌표계, 양의 z축 방향을 바라보고 있으며, 오른쪽이 x축, 위쪽이 y축 방향이다. 일반적으로 Scale의 변화는 주지 않는다.

World space에서 View space로 전환할 때는 View space의 World matrix의 역행렬을 곱하며, 이 변환을 View transform(시야 변환), 행렬을 View matrix(시야 행렬)이라고 한다.

DirectX에서는 XMMATRIX XM_CALLCONV XMMatrixLookAtLH(FXMVECTOR EyePosition, FXMVECTOR FocusPosition, FXMVECTOR UpDirection) 또는 XMMATRIX XM_CALLCONV XMMatrixLookToLH(FXMVECTOR EyePosition, FXMVECTOR EyeDirection, FXMVECTOR UpDirection)를 이용하여 View Matrix를 구할 수 있다. LH는 왼손 좌표계를 의미, LookAt은 바라볼 위치를 위치벡터로 지정, LookTo는 바라볼 방향을 벡터로 지정한다. 카메라가 향하는 방향과 상향 벡터(up vector)는 수직일 필요는 없다.

### 5.6.3 투영과 동차 절단 공간

카메라에 보이는 공간은 하나의 사각뿔대(frustum)으로 정의, 이를 절두체라고 함

3차원 장면을 2차원으로 변환하기 위하여 투영(projection)을 수행

이 책에서는 투영의 한 종류인 Perspective projection(원근 투영)에 대하여 다룬다. 다른 종류로는 Orthographic projection(정사영)이 있다. Perspective projection은 eye point(시점)을 기준으로 한다. Eye point와 정점을 이은 직선을 투영선(projection line)이라고 한다.

Perspective projection에서 절두체는 네 가지의 요소로 정의된다. 원점(시점)과 가까운 평면 사이의 거리 n, 먼 평면 사이의 거리 f, 수직 시야각 a, 종횡비 r이다. 일반적으로 종횡비 r은 후면 버퍼의 종횡비와 일치한다. 종횡비는 (너비/높이)로 정의한다.

(이후는 필기자료 참고)

## 5.7 테셀레이션(자세한 것은 14장에서)

테셀레이션은 주어진 삼각형을 쪼개어 새로운 삼각형들을 만드는 것을 말한다. 이를 통해 더 자세한 묘사가 가능하다.

테셀레이션을 사용하면 가까운 삼각형들에는 테셀레이션을 적용하고 먼 삼각형들에는 적용하지 않는 방식의 LOD(level of detail)을 적용할 수 있으므로 효율적이다. 또한 메모리에 low poly 모델을 저장한 다음 그로부터 즉석에서 삼각형을 추가하므로 메모리를 절약할 수 있다. 마지막으로 물리 등의 연산은 low poly 메시에 대해서만 수행하고 high poly 모델은 렌더링에 사용하여 계산량을 줄일 수 있다.

## 5.8 기하 셰이더(자세한 것은 12장에서)

기하구조들을 GPU에서 생성하거나 파괴

## 5.9 절단

(필기자료 참고)

## 5.10 래스터화 단계(rasterization)

### 뷰포트 전환

뷰포트 전환시 깊이 값은 변경하지 않음, 하지만 뷰포트의 멤버인 MinDepth와 MaxDepth를 변경하여 영향을 미칠 수는 있음, MinDepth와 MaxDepth의 값은 0이상 1이하

### 후면 선별(backface culling)

디폴트 값은 카메라를 기준으로 시계방향으로 감기는 것을 전면, 반시계방향은 후면으로 간주. 하지만 설정에 따라 반대로 하는 것도 가능. 후면 삼각형들은 렌더링할 필요가 없으므로 폐기

이렇게 하면 그려야 할 삼각형의 개수가 대략 절반으로 줄어든다.

### 정점 특성의 보간

하나의 직선을 등간격으로 나누었을 때 투영 평면을 기준으로 기울어져 있다면 비선형적으로 투영됨(투영 후에는 균등하지 않음)

이는 원근 보정 보간(perspective correct interpolation)을 이용하여 정확하게 투영할 수 있음. 자세한 내용은 다루지 않는다.

각 정점 특성들이 삼각형을 따라서 선형으로 보간된다.

## 5.11 픽셀 셰이더

픽셀 셰이더는 각각의 픽셀 단편(pixel fragment, 픽셀의 최종적인 색상을 계산하기 위해 중간적인 픽셀 자료를 말함. 픽셀 단편은 다양한 일을 할 수 있음)에 대해서 적용된다. 일부 픽셀 단편들은 기각될 수 있으며(예를 들면 깊이 판정이나 스텐실 판정에 의해), 그렇지 않은 경우 후면 버퍼에 기록된다. 이 과정에서 혼합이 일어난다. 혼합에 대해서는 10장에서 자세하게 다룬다.
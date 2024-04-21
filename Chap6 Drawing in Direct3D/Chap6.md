# 6장 Direct3D의 그리기 연산

해당 챕터에서는 D3D API를 이용하여 렌더링 파이프라인을 구성하고, 정점 셰이더와 픽셀 셰이더를 정의하고, 기하구조를 렌더링 파이프라인에 제출해서 3차원 물체를 그리는 데에 집중한다.

## 6.1 정점과 입력 배치

D3D에서는 정점에 공간적 위치 이외의 정보들을 추가할 수 있다. 따라서 프로그래머는 커스텀 정점 형식(vertex format)을 만들어야 하며, 이는 구조체로 정의할 수 있다. 예를 들면

```C++
struct Vertex1
{
    XMFLOAT3 Pos;
    XMFLOAT4 Color;
};
struct Vertex2
{
    XMFLOAT3 Pos;
    XMFLOAT3 Normal;
    XMFLOAT2 Tex0;
    XMFLOAT2 Tex1;
};
```

첫 번째 예시의 경우 정점에 XMFLOAT3으로 위치를, XMFLOAT4로 128비트 색을 지정했다. 두 번째의 경우 위치, 법선 벡터, 두 개의 2차원 텍스쳐 좌표를 담았다.

정점 구조체를 정의한 다음에는 그 형태를 D3D에 알려주어야 한다(D3D는 구조체가 어떻게 이루어져 있는지 알 수 없음). 이는 D3D12_INPUT_LAYOUT_DESC를 통해 할 수 있다. D3D12_INPUT_LAYOUT_DESC는 D3D12_INPUT_ELEMENT_DESC의 배열을 가리키는 포인터와 배열에 들어있는 원소의 개수를 담은 구조체이다.
D3D12_INPUT_ELEMENT_DESC의 구성 요소:

1. `LPCSTR(=char\*) SemanticName`: 해당 성분에 부여된 이름, C++상에서의 변수명과는 별개이며, 정점 셰이더의 매개변수에 대응시키는 역할이므로 정점 셰이더 코드 상의 이름과 동일해야 한다.
2. `UINT SemanticIndex`: 하나의 정점 구조에 여러 개의 동일한 요소들을 넣을 경우 구분하기 위한 인덱스이다. 예를 들면 위의 예시에서 `Vertex2`의 `Tex0`과 `Tex1`을 구분하기 위해 `SemanticName`을 다르게 해도 되지만 `SemanticName`을 동일하게 한 다음 `SemanticIndex`만 변경하면 구분할 수 있다. 정점 셰이더 코드 상에서는 `SemanticName+SemanticIndex`로 인식되며, `SemanticIndex`가 없을 경우 자동으로 0이 지정된다.
3. `DXGI_FORMAT Format`: 해당 요소가 어떠한 데이터를 담고 있는지를 알려주는 요소이다. 형식은 `DXGI_FORMAT_R(숫자)G(숫자)B(숫자)A(숫자)\_(각 데이터의 자료형)`이다. 뒤에서부터는 생략이 가능하다. 예를 들면 32비트 실수가 1개인 경우 `DXGI_FORMAT_R32_FLOAT`으로 나타내고, 4차원 32비트 실수 벡터의 경우 `DXGI_FORMAT_R32G32B32A32_FLOAT`으로 나타낸다. `XMFLOAT3`과 대응되는 형식은 `DXGI_FORMAT_R32G32B32_FLOAT`이다. RGBA의 형태로 나타내기는 하지만 색상을 저장할 필요는 없다.
4. `UINT InputSlot`: 정점을 공급받는 슬롯의 번호, 여기서는 숫자 0만 사용한다. 6장 연습문제 2에서 입력 슬롯을 여러 개 사용해본다.
5. `UINT AlignedByteOffset`: 구조체의 시작 위치로부터 해당 변수가 시작하는 위치 사이의 바이트 거리를 말한다. 오프셋은 각 변수의 주소를 출력해보거나 char형 포인터로 강제 형변환해서 차이를 출력하면 쉽게 알 수 있다.
6. `D3D12_INPUT_CLASSIFICATION InputSlotClass`: 일단 현재는 여기에 `D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA`를 지정한다. 다른 값은 인스턴싱에 사용되는데 인스턴싱은 16장에서 학습한다.
7. `UINT InstanceDataStepRate`: 일단 지금은 0을 지정하고 나중에 인스턴싱에서 다른 값을 쓴다.

## 6.2 정점 버퍼

GPU가 정점들의 배열에 접근하기 위해서는 GPU 자원(ID3D12Resource)인 버퍼에 넣어 두어야 한다. 정점들을 저장한 버퍼를 정점 버퍼라고 한다. 버퍼는 텍스처보다 단순한 자원이므로 1차원이며 밉맵이나 필터, 다중표본화 기능이 없다. 응용 프로그램에서 정점 같은 자료 원소들의 배열을 GPU에 제공해야 할 때에는 항상 버퍼를 사용한다.
정점 버퍼를 생성하려면 ID3D12Device::CreateCommittedResource 메서드를 호출한다(이는 4.3.8에서 설명함). CreateCommittedResource를 호출하기 위해서는 D3D12_RESOURCE_DESC를 채워넣어야 하는데, DX12는 D3D12_RESOURCE_DESC를 상속하여 편의용 생성자들과 메서드들을 추가한 CD3DX12_RESOURCE_DESC를 제공한다. 특히 해당 클래스 하위에 있는 Buffer라는 메서드를 이용하면 버퍼를 서술하는 D3D12_RESOURCE_DESC를 바로 만들어낼 수 있다.

```C++
static inline CD3DX12_RESOURCE_DESC Buffer(UINT64 width, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, UINT64 alignment = 0)
```

여기서 width는 버퍼에 들어갈 원소의 개수가 아닌 버퍼의 바이트 개수를 전달해야 한다.
참고: D3D12의 모든 자원들은 모두 ID3D12Resource 인터페이스로 대표된다.

정적 기하구조(프레임마다 변하지 않는 기하 구조)를 그릴 때에는 성능을 위해서 정점 버퍼들을 기본 힙(D3D12_HEAP_TYPE_DEFAULT)에 저장한다. 정적 기하구조의 경우 정점 버퍼에는 GPU만 접근하므로 CPU가 기본 힙에 있는 정점 버퍼를 초기화할 방법이 필요하며, 이는 임시 업로드 버퍼 리소스(D3D12_HEAP_TYPE_UPLOAD)를 생성하여 해결한다. 따라서 정점 자료는 시스템 메모리 -> 업로드 버퍼 -> 정점 버퍼 순으로 이동이 발생한다.
이번 교재에서 제공하는 d3dUtil::CreateDefaultBuffer을 이용하면 기본 버퍼를 초기화할 수 있다.
정점 버퍼를 파이프라인에 묶기 위해서는 서술자가 필요(서술자 힙은 필요 X), 이는 D3D12_VERTEX_BUFFER_VIEW_DESC로 대표된다.
이후에는 커맨드 리스트의 IASetVertexBuffers로 파이프라인에 묶는다. StartSlot을 시작으로 NumBuffers개만큼의 슬롯을 사용한다(이 책에서는 슬롯을 1개만 사용). pViews에는 NumBuffers개만큼의 정점 버퍼 뷰가 있어야 한다.
정점들을 실제로 그리려면 DrawInstanced 메서드를 호출한다.

## 6.3 색인과 색인 버퍼

정점 버퍼와 마찬가지로 색인 버퍼 또한 CreateDefaultBuffer로 생성할 수 있다. 인덱스 버퍼 또한 서술자 힙은 필요 없지만 서술자는 만들어야 한다. IASetIndexBuffer 메서드를 이용해서 파이프라인에 묶고, DrawIndexedInstanced를 이용하여 기본도형을 그린다. DrawIndexedInstance의 각 매개변수들은 여러 개의 정점 버퍼와 색인 버퍼를 이어붙였을 때 중요한 역할을 한다.

## 6.4 예제 정점 셰이더

셰이더는 HLSL(High Level Shading Language)로 작성한다. HLSL은 C++과 문법이 비슷하다.
여기서 정점 셰이더는 국소 공간에서 동차 절단 공간으로 변환하는 역할을 한다.
기하 셰이더를 사용하지 않을 경우 정점 셰이더의 출력에는 반드시 의미소가 SV_POSITION인 동차 절단 공간 상에서의 정점 위치가 포함되어야 한다.

### 6.4.1 입력 배치 서술과 입력 서명 연결

정점 셰이더가 필요한 자료들을 모두 공급했을 때 오류가 발생하지 않는다. 즉, 정점 셰이더에 필요 없는 정보들을 공급하는 것으로는 오류가 발생하지 않는다. 또한, float에 int를 공급하는 등의 자료형이 변환되는 경우에도 경고를 띄울 뿐 오류를 발생시키지는 않는다.
대소문자는 구분하지 않는 듯하다.

## 6.5 예제 픽셀 셰이더

여기서는 입력된 색을 그대로 출력하므로 삼각형을 따라서 보간된 색상값이 출력된다.
픽셀 셰이더의 반환값은 의미소가 SV_TARGET인 4차원 색상값이어야 하며 렌더 타겟의 형식과 일치해야 한다.

## 6.6 상수 버퍼

### 6.6.1 상수 버퍼의 생성

상수 버퍼는 셰이더 프로그램에서 참조하는 자료를 담는 GPU 자원 중 하나이다. 상수 버퍼는 프레임마다 갱신될 수 있으므로(이동 등) 자원을 쉽게 갱신할 수 없는 기본 힙이 아닌 업로드 힙에 생성해야 한다. 상수 버퍼는 프레임마다 CPU가 한 번씩 갱신하는 것이 일반적이다.
상수 버퍼의 크기는 반드시 256바이트(최소 하드웨어 할당 크기)의 배수이어야 한다.
상수 버퍼는 여러 개를 사용해야 하는 경우가 대부분이다. 예를 들어 물체별로 행렬을 둔다면 물체의 개수만큼 행렬이 필요하므로 그만큼 상수 버퍼를 할당해야 한다.
어떤 물체를 그릴 때가 되면 필요한 데이터가 있는 영역을 서술하는 상수 버퍼 뷰를 파이프라인에 묶는다.
HLSL 상에서 상수 버퍼를 256바이트의 배수로 만드는 작업은 암묵적으로 이루어진다. 명시적으로 256바이트의 배수 크기로 만드는 것 또한 가능하다.
D3D12에서는 셰이더 모델 5.1을 도입했으며, 구조체를 정의한 다음 이것을 이용하여 상수 버퍼로 정의할 수 있다.

### 6.6.2 상수 버퍼의 갱신

상수 버퍼를 업로드 힙에 생성했으므로 CPU에서 상수 버퍼의 자원에 자료를 올릴 수 있다. 이를 위해서는 포인터를 얻어야 하므로 ID3D12Resource의 Map 메서드를 이용한다.

참고 자료: https://computergraphics.stackexchange.com/questions/6081/how-does-id3d12resourcemap-work

ID3D12Resource 인터페이스의 Map 메서드는 그래픽 드라이버가 구현한다. Map 함수 호출시 지정한 GPU 자원의 범위를 CPU의 RAM과 1대1 대응을 시켜주며, CPU측의 자료를 변경하면 GPU측의 자원도 동일하게 변경되는 것이다.

상수 버퍼의 사용을 완료했다면 Unmap 메서드로 CPU 메모리와의 대응을 해제한다.

### 6.6.3 업로드 버퍼 보조 클래스

해당 교재에서는 업로드 버퍼를 사용하기 위해 UploadBuffer 클래스를 정의한다. 이 클래스는 업로드 버퍼의 생성과 파괴를 도와주며, 특정한 원소를 갱신하는 CopyData 메서드를 제공한다. 이 클래스를 상수 버퍼에 사용하기 위해서는 isConstantBuffer에 true를 전달해야 한다.
이번 Box 예제에서는 World 행렬, View 행렬, Projection 행렬 총 3개의 행렬을 곱한 결과를 상수 버퍼에 저장한다. Box 예제에서는 세계 좌표와 국소 좌표가 동일하기 때문에 World 행렬에는 항등행렬을 놓는다. Projection 행렬의 경우 OnResize 함수에서 세로 시야각이 45도가 되도록 정의한다. 마지막으로 OnMouseMove에서 클릭한 상태로 움직일 경우 회전과 앞뒤로 움직인 다음 그에 맞는 카메라의 좌표를 계산하여 View 행렬을 프레임마다 갱신한다.

### 6.6.4 상수 버퍼 서술자

상수 버퍼 뷰는 D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV 형식의 서술자 힙에 담아야 한다. 기존의 서술자 힙을 생성하는 과정과 다르게 여기서는 셰이더 프로그램에서 힙에 접근할 것임을 나타내는 D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE을 지정한다는 것이다.
이후에는 상수 버퍼 뷰를 생성한다(CreateConstantBufferView).

### 6.6.5 루트 서명과 서술자 테이블

셰이더 프로그램은 필요한 자원들(상수 버퍼, 텍스처, 표본추출기 등)이 특정한 레지스터 슬롯에 있다고 가정하고 작동한다. 루트 서명(root signature)은 응용 프로그램이 렌더링 파이프라인에 묶어야 하는 자원이 무엇이고 그 자원들이 어떤 셰이더 입력 레지스터들에 대응되는지를 정의한다. 루트 서명은 그리기 호출에 쓰이는 셰이더들이 요구하는 자원을 모두 제공해야 한다.
루트 서명은 루트 매개변수(root parameter)들의 배열이다. 루트 매개변수는 하나의 루트 상수, 루트 서술자, 서술자 테이블 중 하나이다. 루트 상수와 루트 서술자에 대해서는 7장에서 자세하게 다루며, 여기서는 서술자 테이블만 사용한다.
Box 예제에서는 CBV(상수 버퍼 뷰) 하나를 담은 서술자 테이블 하나로 된 루트 매개변수를 이용하여 루트 서명을 생성한다. 루트 매개변수를 작성한 다음에는 루트 서명에 대한 description(DESC)를 작성한다.
D3D12_ROOT_SIGNATURE_DESC 구조체의 나머지 인자들은 이름을 보면 역할을 알 수 있으므로 플래그에 대해서만 알아본다. 플래그를 통해 특정한 셰이더의 루트 서명 접근을 막거나 입력 조립기를 활성화할 수 있다. 여기서는 ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT을 지정했는데, 이는 입력 조립기를 사용할 것임을 의미한다. 만약 사용하지 않을 예정이라면 이 플래그를 생략하여 최적화를 할 수 있지만, Docs에 의하면 그 효과는 미미하다고 한다.
CreateRootSignature 함수를 호출하기 위해서는 DESC 객체를 직렬화(serialize)해야 한다. 직렬화하는 함수(D3D12SerializeRootSignature)를 호출한 다음 직렬화된 객체를 CreateRootSignature 함수에 넘겨 루트 서명을 생성한다.
여담으로 D3D12SerializeRootSignature 함수는 D3D12SerializeVersionedRootSignature 함수로 대체되었다고 한다.
마지막으로 command list에서 SetGraphicsRootDescriptorTable을 호출하여 자원들을 파이프라인에 묶는다.

## 6.7 셰이더의 컴파일

D3D에서 셰이더들은 바이트코드로 컴파일된 후 그래픽 드라이버가 각 GPU에 맞게 네이티브 명령들로 컴파일한다. 셰이더를 컴파일하기 위해서는 D3DCompileFromFile 함수를 호출한다. 매개변수들 중 알아둬야 할 것은 파일 이름, 진입점(사용할 함수의 이름) 타겟(사용할 셰이더 프로그램의 종류와 버전), 그리고 Flags1(세부적인 컴파일 방식)이다. 마지막으로 ID3DBlob\*\* 형식의 매개변수 2개가 있으며, 각각 바이트코드 데이터가 들어있는 주소값을 반환할 위치, 오류가 발생할 경우 오류 문자열의 주소값을 반환할 위치를 의미한다. 여기서는 d3dUtil의 CompileShader 함수를 이용하여 셰이더를 컴파일한다.

ID3DBlob은 범용 메모리 버퍼를 나타내는 형식이며, GetBufferPointer(버퍼를 가리키는 포인터)와 GetBufferSize(버퍼의 바이트 크기)의 두 가지 메서드를 제공한다.

### 6.7.1 오프라인 컴파일

오프라인 컴파일은 셰이더를 실행 시점 이전에 컴파일하는 것을 의미한다. 오프라인 컴파일을 하면 오류를 빨리 확인할 수 있고 로딩 시간이 적어지는 장점이 있다.
오프라인 컴파일은 Windows SDK에 들어있는 FXC를 이용하여 할 수 있다. FXC의 자세한 사용 방법은 교재 284페이지를 확인하면 된다.
오프라인 컴파일을 완료했다면 해당 목적 파일을 ID3DBlob 형태로 불러오면 된다.

### 6.7.2 어셈블리 코드 생성

어셈블리 코드를 확인하여 실제로 코드가 어떻게 작동하는지 보기 위한 기능이다.

### 6.7.3 Visual Studio를 이용한 오프라인 셰이더 컴파일

비주얼 스튜디오에 HLSL파일을 추가하면 자동으로 컴파일해준다. 이 기능의 단점은 하나의 HLSL파일 당 하나의 목적 코드만 생성이 가능하다는 것이다. 따라서 하나의 파일로부터 여러 가지의 목적 파일을 얻는 것은 불가능하다.

## 6.8 래스터라이저 상태

렌더링 파이프라인 중 래스터화 단계는 프로그래밍이 불가능하고 설정만 가능하다. 래스터화 단계는 래스터라이저 상태(rasterizer state, D3D12_RASTERIZER_DESC)를 통해서 구성한다. D3D12_RASTERIZER_DESC의 요소로는 여러 가지가 있지만, 여기서는 4가지만 알아본다.

1. FillMode: D3D12_FILL_MODE_WIREFRAME 또는 D3D12_FILL_MODE_SOLID를 지정한다. 기본은 SOLID.
2. CullMode: D3D12_CULL_NONE 또는 D3D12_CULL_BACK 또는 D3D12_CULL_FRONT를 지정한다. 기본은 BACK.
3. FrontCounterClockwise: false면 시계방향으로 감긴 것이 정면, true면 반대이다. 기본은 false.
4. ScissorEnable: 가위 판정(화면 밖의 픽셀들을 제외하는 과정)의 활성화 여부. 기본은 false.

인자들이 매우 많지만 편의용 클래스인 CD3DX12_RASTERIZER_DESC을 이용하면 쉽게 생성할 수 있다. 이 생성자에 D3D12_DEFAULT(=CD3D12_DEFAULT, 몸체에는 아무것도 없으며 단순히 기본값을 채우는 생성자를 오버로딩 하기 위해 있는 구조체)를 전달하면 모두 기본값을 채워준다.

## 6.9 파이프라인 상태 객체(Pipeline State Object, PSO)

파이프라인 상태 객체는 여러 가지 상태들을 모든 객체이며 렌더링 파이프라인의 상태를 제어할 수 있다. D3D12_GRAPHICS_PIPELINE_STATE_DESC 구조체를 채워넣어야 한다. 이 서술 구조체에는 굉장히 많은 멤버들이 있는데, 이는 성능을 위해서이다. 파이프라인 상태의 대부분을 하나의 객체로 지정하여 최적화를 할 수 있다(DirectX 11에서는 각 상태들을 개별적으로 컨트롤했다).
PSO의 검증과 생성에는 많은 시간이 걸릴 수 있으므로 초기화 시점에 생성하는 것이 맞다.
PSO는 뷰포트나 가위 직사각형과 같이 독립적으로 지정해도 괜찮은 상태들은 포함하지 않는다.
Command list를 Reset하는 시점에서 초기 PSO를 지정할 수 있다. 하나의 PSO를 command list에 묶었다면 새로운 PSO를 지정하기 전까지는 해당 PSO를 사용한다. 성능을 위해서는 PSO의 변경을 최소화하는 것이 좋으므로 하나의 PSO를 최대한 활용하는 것이 좋다.

## 6.10 기하구조 보조 구조체(MeshGeometry)

이 교재에서 사용하는 보조 구조체로, 정점 버퍼와 색인 버퍼를 하나로 묶었다. 이 구조체는 실제 정점 자료와 색인 자료를 메모리에 유지한다. 이는 CPU가 충돌 판정이나 선택 등을 수행할 수 있도록 하기 위해서이다. 여기에는 정점 버퍼와 색인 버퍼의 주요 속성들을 저장하며 버퍼에 대한 뷰를 반환하는 메서드 또한 있다. 하나의 MeshGeometry 객체에는 여러 개의 기하 구조들을 저장할 수 있는데, 이것은 SubmeshGeometry로 구현한다. SubmeshGeometry에는 IndexCount, StartIndexLocation, BaseVertexLocation의 세 가지 속성들이 있으며, 이에 대한 내용으로는 색인 버퍼의 내용을 참고하면 된다(p.256). 하나의 MeshGeometry 안에 들어있는 SubmeshGeometry 데이터는 모두 unordered_map에 문자열을 키로 저장되며, 이후에 검색할 수 있다.

## 6.11 Box 예제

WorldViewProj 행렬을 상수 버퍼를 통해 셰이더로 전달할 때 Transpose를 해주어야 하는 이유는 DirectX의 XMMATRIX와 HLSL의 float4x4의 데이터 해석 방식이 다르기 때문이다.
예를 들어 데이터가 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 순으로 들어있을 때 XMMATRIX의 경우 $\begin{bmatrix}0&1&2&3\\4&5&6&7\\8&9&10&11\\12&13&14&15\end{bmatrix}$로 해석하며, 이를 row-major ordering이라고 한다. 즉, 앞에서부터 4개씩을 하나의 행으로 해석하는 것이다. 반면에 HLSL은 같은 형태의 데이터를 $\begin{bmatrix}0&4&8&12\\1&5&9&13\\2&6&10&14\\3&7&11&15\end{bmatrix}$로 해석하며, 이를 column-major ordering이라고 한다. DirectX에서 HLSL로 데이터를 넘겨줄 때는 그대로 넘겨주므로 Transpose를 한 번 해서 넘겨주어야 한다.

# 6장을 마치며

Default Buffer쪽 복습을 한 번 해야 할 듯하다.

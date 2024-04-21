# Chap10 혼합

기존에는 깊이 판정에 성공할 경우(=물체가 더 앞에 있는 경우) 후면 버퍼에 있는 픽셀을 덮어썼다. 하지만 반투명한 물체를 그리기 위해서는 덮어쓰면 안 되며, 기존에 있는 픽셀의 데이터를 활용하여 새로운 데이터와 섞는 혼합(blending) 과정이 필요하다.

## 목표

1. 혼합의 작동 방식과 D3D에서 혼합을 사용하는 법을 이해한다.
2. D3D가 지원하는 여러 혼합 모드를 배운다.
3. 기본도형의 투명도를 알파 성분을 이용해서 제어하는 방법을 파악한다.
4. HLSL의 clip 함수를 이용해 한 픽셀이 후면 버퍼에 아예 그려지지 않게 만드는 방법을 배운다.

## 10.1 혼합 공식

원본 픽셀(현재 래스터화 중인 픽셀)에 대해 픽셀 셰이더가 출력한 색상을 $C_{src}$라고 하고 현재 동일한 위치의 후면 버퍼에 있는 색상을 $C_{dst}$라고 하자. 혼합이 없고 깊이 판정을 통과했다면 $C_{src}$가 $C_{dst}$를 덮어쓸 것이다.

혼합을 적용할 경우 $C_{src}$와 $C_{dst}$에 일정한 공식을 적용하여 새로운 색상 $C$를 생성하여 $C_{dst}$를 덮어쓰게 된다. D3D에서 원본 픽셀과 대상 픽셀을 혼합할 때 쓰는 일반적인 공식은 다음과 같다.

$C=C_{src} \bigotimes F_{src} \boxplus C_{dst} \bigotimes F_{dst}$

여기서 ⊗는 성분별 곱셈을 의미하고 ⊞는 덧셈, 뺄셈 등의 뒤에서 설명할 여러 이항 연산자들을 묶어서 표시한 것이다. F*{src}와 F*{dst는} 혼합 계수(blending factor)이라고 한다. 이 혼합 공식은 RGB 성분에만 적용된다.

$A=A_{src} F_{src} \boxplus A_{dst} F_{dst}$

알파 채널은 RGB와는 따로 연산과 계수를 적용할 수 있다. 알파 채널과 RGB 채널을 분리한 이유는 둘을 따로 처리하여 다양한 방식의 혼합을 가능하도록 하기 위해서이다(알파 성분의 혼합은 RGB 혼합에 비해서는 덜 쓰인다고 한다).

## 10.2 혼합 연산

이항 연산자 ⊞로 가능한 것들은 총 5가지이며 그 결과는 다음과 같다.

$
C=C_{src} \bigotimes F_{src}+C_{dst} \bigotimes F_{dst}\\
C=C_{src} \bigotimes F_{src}-C_{dst} \bigotimes F_{dst}\\
C=C_{dst} \bigotimes F_{dst}-C_{src} \bigotimes F_{src}\\
C=min⁡(C_{src},C_{dst})\\
C=max⁡(C_{src},C_{dst})
$

앞에서 언급한대로 RGB 채널에 적용할 연산자와 알파 채널에 적용할 연산을 다르게 할 수 있다.

최근의 D3D에서는 논리 연산자를 사용하여 혼합을 하는 기능이 추가되었다. 다만 기존의 5가지 연산자와 동시에 사용할 수는 없다. 또한 픽셀 형식이 부호 없는 정수 형식이어야 한다. 그렇지 않을 경우 오류 메시지를 출력한다.

## 10.3 혼합 계수

https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_blend

위의 문서를 참고하면 된다. 옵션을 어떤 것으로 지정하는지에 따라서 다양한 효과를 줄 수 있다. 책에서는 0번부터 15번까지 언급하며 $F_{src}$와 $F_{dst}$ 모두에 적용할 수 있는 계수들이다. RGB에서는 모두 사용 가능하며, 알파 혼합에서는 COLOR로 끝나는 계수들은 사용이 불가능하다.

혼합 계수가 색상을 전달받아야 하는 경우 command list의 OMSetBlendFactor 메서드를 이용하면 된다.

## 10.4 혼합 상태

혼합 계수와 혼합 연산자 등의 혼합 설정을 담은 구조체를 혼합 상태라고 한다. 혼합 상태는 PSO의 일부이며 그동안은 혼합이 비활성화된 기본 혼합 상태를 사용했다.

혼합 상태를 설정하려면 D3D12_BLEND_DESC 구조체를 채워야 한다. 멤버는 3개이다.

1. BOOL AlphaToConvergeEnable: true로 설정하면 알파 포괄도(alpha-to-converge) 변환이 활성화된다. 이는 창살 등을 렌더링할 때 유용한 멀티샘플링 기법이라고 하며, 그렇기에 멀티샘플링이 활성화되어 있어야 한다.
2. BOOL IndependentBlendEnable: D3D에서 한 번에 렌더링할 수 있는 렌더 타겟은 8개이다. 이 플래그가 true이면 각 렌더 타겟에 다른 혼합을 적용할 수 있고, false면 모든 타겟에 첫 번째(RenderTarget[0]) 혼합 공식이 적용된다.
3. D3D12_RENDER_TARGET_BLEND_DESC RenderTarget[8]: 각 렌더 타겟에 적용할 혼합 설정 구조체 배열이다.

D3D12_RENDER_TARGET_BLEND_DESC에 관한 설명은 API 문서(https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_render_target_blend_desc)를 참고한다. 여기에는 혼합과 논리 연산자 혼합 중 어떤 것을 사용할 것인지(모두 사용하지 않을 수도 있음), 총 4개(원본/대상, RGB/알파)의 혼합 계수, 2개(RGB/알파)의 혼합 연산자, 논리 연산자 혼합을 사용하는 경우 논리 연산자, 마지막으로 혼합 결과를 기록할 후면 버퍼 색상 채널을 지정하는 플래그가 있다. 플래그의 경우 RGBA 순으로 1,2,4,8이며, OR 연산자를 통해 둘 이상을 지정하거나 ALL 플래그를 통해 모두 지정할 수 있다.

혼합은 추가 비용이 발생하므로 필요한 경우에만 혼합을 활성화하고 그 외에는 비활성화해야 한다.

초기화 시점에 모든 PSO를 만들어놓은 다음 필요에 따라서 PSO를 전환한다.

## 10.5 예제

### 10.5.1 색상 기록 금지

대상 픽셀을 그대로 유지하고 싶다면 $F_{src}$를 ZERO, $F_{dst}$를 ONE으로 설정하면 된다. 다만 이와 같은 경우에는 RenderTargetMask를 0으로 지정하는 것도 가능하다.

### 10.5.2 가산 혼합과 감산 혼합

가산 혼합은 두 색상을 더하는 것으로 $F_{src}$와 $F_{dst}$를 모두 ONE으로 지정하고 연산자를 ADD로 지정하면 된다.
감산 혼합은 빼는 것이므로 연산자를 SUBTRACT로 지정하면 된다.

### 10.5.3 승산 혼합

대상 픽셀과 원본 픽셀의 성분별 곱셈이다. 혼합한 결과가 $C_{src} \bigotimes C_{dst}$가 되도록 조절하면 된다.

### 10.5.4 투명도

알파 성분이 불투명도(opacity)를 제어하는 것이라고 한다면 투명도(transparency)는 1-불투명도이다. 원본 픽셀은 원본 픽셀의 불투명도만큼, 대상 픽셀은 원본 픽셀의 투명도만큼 혼합한다면 반투명한 물체를 그릴 수 있다.

투명한 물체를 그릴 경우에는 다음과 같은 규칙을 사용한다. 먼저 혼합을 사용하지 않는 물체들(불투명한 물체)을 먼저 그린다. 그 다음, 혼합을 사용하는 물체들을 카메라와의 거리에 따라 정렬하고 카메라에서 먼 것부터 가까운 순으로 그려야 한다. 투명도를 적용했을 때 혼합하는 순서를 변경하게 되면 결과값이 달라진다.

반면에 가산, 감산, 승산 혼합은 불투명한 물체들을 먼저 그려야 하는 것은 동일하지만 순서는 크게 상관없다. 왜냐하면 덧셈과 곱셈은 교환법칙이 성립하기 때문이다.

### 10.5.5 혼합과 깊이 버퍼

가산, 감산, 승산 혼합에서는 물체를 그리는 순서는 상관이 없다고 했지만, 이럴 경우 깊이 판정에서 문제가 발생한다. 예를 들어, 두 개의 물체를 혼합할 때 둘 중 앞에 있는 것이 먼저 혼합이 되었다면 깊이 버퍼에는 해당 물체의 깊이가 들어가게 되고, 따라서 뒤에 있는 물체는 깊이 판정에 실패하여 반영이 되지 않는 것이다. 따라서 혼합할 물체들을 렌더링할 때에는 깊이 버퍼 기록을 비활성화해야 한다. 단, 깊이 읽기와 깊이 판정은 그대로 두어야 한다. 예를 들어 혼합할 두 물체 앞에 벽이 가로막고 있다면 렌더링하면 안 되기 때문이다. 깊이 판정 설정 방법은 다음 장에서 설명한다.

## 10.6 알파 채널

기본 픽셀 셰이더는 알파 채널을 분산광 재질에서 가지고 온다.

포토샵 등의 이미지 편집 프로그램에서 알파 채널을 추가할 수 있다.

## 10.7 픽셀 잘라내기

종종 원본 픽셀을 완전히 기각(reject)해서 더 이상의 처리가 일어나지 않게 해야 할 때가 있다. 이를 픽셀 잘라내기(clipping)라고 하며, 이를 수행하는 한 가지 방법은 HLSL 함수 clip을 사용하는 것이다. clip 함수는 전달된 값이 0보다 작으면 픽셀을 폐기하여 더 이상의 처리가 일어나지 않는다. 이 함수는 철망처럼 완전히 불투명한 픽셀들과 완전히 투명한 픽셀들을 렌더링할 때 유용하다. 픽셀 폐기는 최대한 빨리 수행하여 이후의 추가적인 연산들이 일어나지 않도록 하면 좋다.

픽셀 셰이더 상에서는 매크로 ALPHA_TEST를 활성화했을 때 알파값이 0.1보다 작을 경우 픽셀을 폐기하는 방식으로 작동한다. 필터링에 의해 알파 값이 뭉개지는 경우가 있으므로 완벽히 0이 아니더라도 적당히 작은 값에서 폐기하는 것이 좋다. 픽셀 폐기는 추가 비용이 필요하고 수행하지 않는 것이 바람직한 경우도 있으므로 필요한 경우에만 사용해야 한다.

이번 예제에서는 철망을 렌더링할 때 활성화한다. 여기서는 철망의 한 면을 통해서 뒤쪽에 위치한 면이 보이는 것이 가능하므로 PSO에서 후면 선별을 비활성화해야 한다.

## 10.8 안개

안개 효과는 안개가 낀 모습을 구현할 수 있을 뿐만 아니라 먼 거리에 있는 렌더링 결함이나 물체가 갑자기 튀어나오는 현상을 숨길 수 있다. 따라서 게임에서의 날씨가 맑다고 해도 먼 거리에 약간의 안개를 포함시키는 것이 바람직하다. 맑은 날에도 멀리 있는 물체는 흐릿하게 보이는 대기 원근 현상은 있으므로 이를 흉내내기에 적합하다.

안개 현상을 구현하기 위해 원래 색상과 안개 색상을 혼합한다. 구체적으로는 안개 색상을 s만큼, 원래 색상을 1-s만큼 섞는다. s는 0과 1 사이의 값이며, 카메라 위치와 표면 점 사이의 거리의 함수이다. s는 fogStart에서 0으로 시작하여 fogEnd=fogStart+fogRange에서 1까지 선형적으로 증가하며, fogStart보다 작은 경우에는 0, fogEnd보다 큰 경우에는 1이다. 따라서 fogEnd보다 먼 점들은 안개에 완전히 가려지게 된다. s의 값은 saturate 함수(0보다 작을 때는 0, 1보다 클 때는 1, 그 사이에서는 그대로)를 이용하여 쉽게 구현할 수 있다.

셰이더 코드에서는 FOG 매크로를 정의했을 때 안개를 적용한다. 동일한 셰이더 코드에서 매크로를 다르게 하는 방법은 (매크로 이름, 값) 쌍이 담겨있는 배열을 셰이더 컴파일 시점에 전달하는 것이다.
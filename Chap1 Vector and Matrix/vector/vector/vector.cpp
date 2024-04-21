#include <Windows.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <d3d12.h>
#include <iostream>
#include <dxgi.h>

using namespace std;
using namespace DirectX;
using namespace DirectX::PackedVector;

std::ostream& XM_CALLCONV operator<<(std::ostream& os, FXMVECTOR v)
{
	XMFLOAT3 dest;
	XMStoreFloat3(&dest, v);
	os << '(' << dest.x << ", " << dest.y << ", " << dest.z << ')';
	return os;
}

ostream& XM_CALLCONV operator<<(ostream& os, FXMMATRIX m)
{
	for (int i = 0; i < 4; i++)
	{
		cout << XMVectorGetX(m.r[i]) << '\t';
		cout << XMVectorGetY(m.r[i]) << '\t';
		cout << XMVectorGetZ(m.r[i]) << '\t';
		cout << XMVectorGetW(m.r[i]) << '\t';
		cout << endl;
	}
	return os;
}

struct Vertex
{
	XMFLOAT3 pos;
	XMFLOAT4 color;
};

//int main(void)
//{
//	std::cout.setf(std::ios_base::boolalpha);
//	if (!XMVerifyCPUSupport())
//	{
//		cout << "DirectXMath Unsupported" << endl;
//		return 0;
//	}
//	XMVECTOR p = XMVectorZero();
//	XMVECTOR q = XMVectorSplatOne();
//	XMVECTOR u = XMVectorSet(1.1f, 2.1f, 3.1f, 4.1f);
//	XMVECTOR v = XMVectorReplicate(-2.0f);
//	XMVECTOR w = XMVectorSplatZ(u);
//	cout << p << endl;
//	cout << q << endl;
//	cout << u << endl;
//	cout << v << endl;
//	cout << w << endl;
//
//
//	XMVECTOR a = u;
//	XMMATRIX m = XMMatrixIdentity();
//	cout << XMMatrixRotationX(XM_PI) << endl;
//	cout << XMMatrixRotationAxis(XMVector3Normalize(XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f)), XM_PIDIV2 / 3);
//	cout << m << endl;
//	XMVECTOR projA, perpA;
//	XMVector3ComponentsFromNormal(&projA, &perpA, a, XMVectorSet(1.0f, 1.0f, 0.0f, 0.0f));
//	cout << projA << perpA << endl;
//	cout << XMVector3Equal(projA + perpA, a) << ' ' << XMVector3NearEqual(projA + perpA, a, XMVectorReplicate(1e-6f)) << endl;
//	XMCOLOR c(0.5, 0.5, 0.5, 0.5);
//	XMVECTOR x = XMLoadColor(&c);
//
//	cout << XMMatrixTranspose(XMMatrixPerspectiveFovLH(XM_PI * 0.5f, 1.0f, 1.0f, 100.0f)) << endl;
//
//	cout << x << endl;
//	
//	cout << sizeof(XMFLOAT3) << endl;
//	return 0;
//}

int main(void)
{
	XMMATRIX p = XMMatrixSet(1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6);
	float* ptr = (float*)&p;
	for (int i = 0; i < 16; i++)
		cout << ptr[i] << endl;
	cout << p << endl;

	Vertex v;
	cout << &v.pos << ' ' << &v.color << ' ' << &v << endl;

	return 0;
}
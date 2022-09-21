#include "utils.h"

void utils::census_transform_5x5(const uint8* source, uint32* census,
	const sint32& width, const sint32& height)
{
	if (source == nullptr || census == nullptr || width <= 5u || height <= 5u) {
		return;
	}

	// �����ؼ���censusֵ
	for (sint32 i = 2; i < height - 2; i++) {
		for (sint32 j = 2; j < width - 2; j++) {

			// ��������ֵ
			const uint8 gray_center = source[i * width + j];

			//������СΪ5x5�Ĵ������������أ���һ�Ƚ�����ֵ����������ֵ��С������censusֵ
			uint32 census_val = 0u;
			for (sint32 r = -2; r <= 2; r++) {
				for (sint32 c = -2; c <= 2; c++) {
					census_val <<= 1;
					const uint8 gray = source[(i + r) * width + j + c];
					if (gray < gray_center) {
						census_val += 1;
					}
				}
			}
			census[i * width + j] = census_val;
		}
	}
}

uint32 utils::Hamming32(const uint32& x, const uint32& y)
{
	uint32 dist = 0, val = x ^ y;

	//����������1�ĸ���
	while (val) {
		++dist;
		val &= val - 1;
	}
	return dist;
}

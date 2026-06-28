#version 450
layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout (binding = 0, rgba8) uniform readonly image2D srcMip;
layout (binding = 1, rgba8) uniform writeonly image2D dstMip;

void main() {
    ivec2 globalId = ivec2(gl_GlobalInvocationID.xy);
    ivec2 srcCoords = globalId * 2; // Координаты в предыдущем (большем) мипе

    // 1. Читаем 4 пикселя из srcMip и распаковываем их в нормали [-1, 1]
    // Мы предполагаем, что нормали хранятся в RGB-каналах текстуры
    vec3 n0 = imageLoad(srcMip, srcCoords + ivec2(0, 0)).rgb * 2.0 - 1.0;
    vec3 n1 = imageLoad(srcMip, srcCoords + ivec2(1, 0)).rgb * 2.0 - 1.0;
    vec3 n2 = imageLoad(srcMip, srcCoords + ivec2(0, 1)).rgb * 2.0 - 1.0;
    vec3 n3 = imageLoad(srcMip, srcCoords + ivec2(1, 1)).rgb * 2.0 - 1.0;

    // 2. Складываем нормали и ОБЯЗАТЕЛЬНО нормализуем сумму
    vec3 averagedNormal = normalize(n0 + n1 + n2 + n3);

    // 3. Запаковываем обратно в [0, 1] и записываем в dstMip
    imageStore(dstMip, globalId, vec4(averagedNormal * 0.5 + 0.5, 1.0));
}
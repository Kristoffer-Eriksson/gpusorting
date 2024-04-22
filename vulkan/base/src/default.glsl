#version 460 core

layout(std430, binding = 0) buffer Buffer {
    float data[];
} ssbo;

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
    int n = ssbo.data.length();

    // Bubble sort
    do {
        int newn = 0;

        for (int i = 1; i < n; i++) {
            if (ssbo.data[i - 1] > ssbo.data[i]) {
                float temp = ssbo.data[i - 1];
                ssbo.data[i - 1] = ssbo.data[i];
                ssbo.data[i] = temp;

                newn = i;
            }
        }
        n = newn;
    } while (n > 1);
}

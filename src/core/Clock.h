#pragma once

/** 帧间隔计时（秒） */
struct Clock {
    float lastTime = 0.0f;
    float tick(float nowSeconds) {
        float d = nowSeconds - lastTime;
        lastTime = nowSeconds;
        return (d > 1e-6f) ? d : (1.0f / 60.0f);
    }
};

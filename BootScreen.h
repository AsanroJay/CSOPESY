#pragma once

class BootScreen {
public:
    void draw();

  
    bool isComplete() const;

private:
    double m_startTime = -1.0;  
    bool   m_skipped   = false;
};

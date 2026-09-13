#pragma once

class VMTHook
{
public:
    void** vt = nullptr;
    void* obj = nullptr;
    int count = 0;
    void** orig = nullptr;

    ~VMTHook() { delete[] orig; }

    bool Init(void* obj)
    {
        if (!obj) return false;
        this->obj = obj;
        vt = *(void***)obj;
        MEMORY_BASIC_INFORMATION mbi;
        for (count = 0; count < 128; count++)
        {
            if (!VirtualQuery(vt[count], &mbi, sizeof(mbi)) || mbi.Type != MEM_IMAGE) break;
            DWORD p = mbi.Protect & 0xFF;
            if (p != PAGE_EXECUTE && p != PAGE_EXECUTE_READ &&
                p != PAGE_EXECUTE_READWRITE && p != PAGE_EXECUTE_WRITECOPY) break;
        }
        if (count < 1) return false;
        orig = new void*[count];
        for (int i = 0; i < count; i++) orig[i] = vt[i];
        return true;
    }

    bool Hook(int index, void* fn)
    {
        if (!vt || index < 0 || index >= count) return false;
        DWORD old;
        VirtualProtect(&vt[index], 4, PAGE_EXECUTE_READWRITE, &old);
        vt[index] = fn;
        VirtualProtect(&vt[index], 4, old, &old);
        return true;
    }

    void* GetOriginal(int index) { return orig ? orig[index] : nullptr; }
};

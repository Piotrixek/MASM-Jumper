#ifndef _MAIN_CPP_
#define _MAIN_CPP_

#include <yvals_core.h>

#if !_HAS_CXX23
#error _CXX23_REQUIRED_
#endif

#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

#pragma warning(push)
#pragma warning(disable : 4100 4324)
#pragma pack(push, 8)
#pragma push_macro("new")
#undef new

#define WIN32_LEAN_AND_MEAN
#include <chrono>
#include <expected>
#include <print>
#include <thread>
#include <windows.h>

#define _STD ::std::
#define _CSTD ::
#define _NODISCARD [[nodiscard]]

_STD_BEGIN

struct alignas(32) _Jumper_state
{
    int _Player_x;
    int _Player_y;
    int _Velocity_y;
    int _Max_y;
    int _Score;
    int _Is_alive;
    int _Width;
    int _Height;
    int _Saucers_x[16];
    int _Saucers_y[16];
    unsigned int _Rng_state;
};

enum class _Tile_type : char
{
    _Empty = ' ',
    _Saucer = '=',
    _Player_Up = 'A',
    _Player_Down = 'V'
};

_STD_END

extern "C"
{
    void __stdcall _Update_jumper_physics_asm(_STD _Jumper_state *_State, int _Input) noexcept;
    void __stdcall _Clear_board_asm(void *_Dest, _STD size_t _Qwords) noexcept;
    void __stdcall _Reset_game_asm(_STD _Jumper_state *_State, int _Start_x) noexcept;
    void __stdcall _Initialize_saucers_asm(_STD _Jumper_state *_State) noexcept;
    void __stdcall _Recycle_saucers_asm(_STD _Jumper_state *_State) noexcept;
}

_STD_BEGIN

template <size_t _Width, size_t _Height> class _Jumper_engine
{
  private:
    alignas(64) _Tile_type _Board_data[_Width * _Height];
    _Jumper_state _State;
    HWND _Window_handle;
    bool _Is_running;
    int _App_state;
    bool _Space_was_down;

    static LRESULT CALLBACK _Wnd_proc(HWND _Hwnd, UINT _Msg, WPARAM _Wparam, LPARAM _Lparam) noexcept
    {
        if (_Msg == WM_DESTROY)
        {
            PostQuitMessage(0);
            return 0;
        }
        else if (_Msg == WM_PAINT)
        {
            PAINTSTRUCT _Ps;
            HDC _Hdc = BeginPaint(_Hwnd, &_Ps);
            HDC _Mem_dc = CreateCompatibleDC(_Hdc);
            RECT _Rect;
            GetClientRect(_Hwnd, &_Rect);
            HBITMAP _Mem_bitmap = CreateCompatibleBitmap(_Hdc, _Rect.right, _Rect.bottom);
            HBITMAP _Old_bitmap = static_cast<HBITMAP>(SelectObject(_Mem_dc, _Mem_bitmap));

            _Jumper_engine *_Engine = reinterpret_cast<_Jumper_engine *>(GetWindowLongPtrW(_Hwnd, GWLP_USERDATA));
            if (_Engine)
            {
                _Engine->_Draw_frame(_Mem_dc, _Rect);
            }

            BitBlt(_Hdc, 0, 0, _Rect.right, _Rect.bottom, _Mem_dc, 0, 0, SRCCOPY);
            SelectObject(_Mem_dc, _Old_bitmap);
            DeleteObject(_Mem_bitmap);
            DeleteDC(_Mem_dc);
            EndPaint(_Hwnd, &_Ps);
            return 0;
        }
        else if (_Msg == WM_ERASEBKGND)
        {
            return 1;
        }
        return DefWindowProcW(_Hwnd, _Msg, _Wparam, _Lparam);
    }

  public:
    constexpr _Jumper_engine() noexcept
        : _Window_handle(nullptr), _Is_running(true), _App_state(0), _Space_was_down(false)
    {
        _State._Width = static_cast<int>(_Width);
        _State._Height = static_cast<int>(_Height);
        _State._Rng_state = 1337;
    }

    constexpr ~_Jumper_engine() noexcept = default;
    constexpr _Jumper_engine(const _Jumper_engine &) noexcept = default;
    constexpr _Jumper_engine &operator=(const _Jumper_engine &) noexcept = default;

    void _Draw_frame(HDC _Hdc, RECT _Rect) const noexcept
    {
        FillRect(_Hdc, &_Rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        HFONT _Font = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE, "Consolas");
        HFONT _Old_font = static_cast<HFONT>(SelectObject(_Hdc, _Font));
        SetBkMode(_Hdc, TRANSPARENT);

        if (_App_state == 1)
        {
            for (size_t _R = 0; _R < _Height; ++_R)
            {
                for (size_t _C = 0; _C < _Width; ++_C)
                {
                    const char _Char = static_cast<char>(_Board_data[_R * _Width + _C]);
                    if (_Char == ' ')
                        continue;

                    if (_Char == '=')
                    {
                        SetTextColor(_Hdc, RGB(0, 255, 255));
                    }
                    else
                    {
                        SetTextColor(_Hdc, RGB(255, 255, 0));
                    }
                    TextOutA(_Hdc, static_cast<int>(_C) * 10, static_cast<int>(_R) * 16, &_Char, 1);
                }
            }

            char _Score_buffer[32];
            int _Score_len = wsprintfA(_Score_buffer, "SCORE: %d", _State._Score);
            SetTextColor(_Hdc, RGB(255, 255, 255));
            TextOutA(_Hdc, 10, 10, _Score_buffer, _Score_len);
        }
        else if (_App_state == 0)
        {
            const char _Menu_text[] = "INFINITE JUMPER (C++23 + MASM)";
            const char _Menu_sub[] = "PRESS [SPACE] TO START";
            SetTextColor(_Hdc, RGB(0, 255, 0));
            TextOutA(_Hdc, (_Rect.right / 2) - 150, (_Rect.bottom / 2) - 20, _Menu_text, sizeof(_Menu_text) - 1);
            SetTextColor(_Hdc, RGB(255, 255, 255));
            TextOutA(_Hdc, (_Rect.right / 2) - 110, (_Rect.bottom / 2) + 20, _Menu_sub, sizeof(_Menu_sub) - 1);
        }
        else if (_App_state == 2)
        {
            const char _Over_text[] = "GAME OVER!";
            char _Score_buffer[32];
            int _Score_len = wsprintfA(_Score_buffer, "FINAL SCORE: %d", _State._Score);
            const char _Over_sub[] = "PRESS [SPACE] TO RETURN TO MENU";
            SetTextColor(_Hdc, RGB(255, 0, 0));
            TextOutA(_Hdc, (_Rect.right / 2) - 50, (_Rect.bottom / 2) - 40, _Over_text, sizeof(_Over_text) - 1);
            SetTextColor(_Hdc, RGB(255, 255, 0));
            TextOutA(_Hdc, (_Rect.right / 2) - 70, (_Rect.bottom / 2) - 10, _Score_buffer, _Score_len);
            SetTextColor(_Hdc, RGB(255, 255, 255));
            TextOutA(_Hdc, (_Rect.right / 2) - 150, (_Rect.bottom / 2) + 30, _Over_sub, sizeof(_Over_sub) - 1);
        }

        SelectObject(_Hdc, _Old_font);
        DeleteObject(_Font);
    }

    void _Run() noexcept
    {
        WNDCLASSEXW _Wc = {0};
        _Wc.cbSize = sizeof(WNDCLASSEXW);
        _Wc.style = CS_HREDRAW | CS_VREDRAW;
        _Wc.lpfnWndProc = _Wnd_proc;
        _Wc.hInstance = GetModuleHandleW(nullptr);
        _Wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
        _Wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        _Wc.lpszClassName = L"_Jumper_Class";

        RegisterClassExW(&_Wc);

        _Window_handle =
            CreateWindowExW(0, L"_Jumper_Class", L"MASM x64 Infinite Jumper", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                            CW_USEDEFAULT, CW_USEDEFAULT, static_cast<int>(_Width) * 10 + 30,
                            static_cast<int>(_Height) * 16 + 50, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

        SetWindowLongPtrW(_Window_handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        using namespace _STD chrono_literals;
        auto _Last_tick = _STD chrono::steady_clock::now();

        MSG _Msg;
        while (_Is_running)
        {
            while (PeekMessageW(&_Msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (_Msg.message == WM_QUIT)
                {
                    _Is_running = false;
                }
                TranslateMessage(&_Msg);
                DispatchMessageW(&_Msg);
            }

            int _Input = 0;
            if (GetAsyncKeyState('A') & 0x8000)
                _Input = 'A';
            if (GetAsyncKeyState('D') & 0x8000)
                _Input = 'D';
            if (GetAsyncKeyState('Q') & 0x8000)
                _Is_running = false;

            bool _Space_is_down = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

            if (_App_state == 0)
            {
                if (_Space_is_down && !_Space_was_down)
                {
                    _Reset_game_asm(&_State, static_cast<int>(_Width / 2));
                    _Initialize_saucers_asm(&_State);
                    _App_state = 1;
                }
                InvalidateRect(_Window_handle, nullptr, FALSE);
            }
            else if (_App_state == 1)
            {
                auto _Current_time = _STD chrono::steady_clock::now();
                if (_Current_time - _Last_tick >= 30ms)
                {
                    _Last_tick = _Current_time;

                    _Update_jumper_physics_asm(&_State, _Input);

                    if (_State._Is_alive == 0)
                    {
                        _App_state = 2;
                    }
                    else
                    {
                        _Recycle_saucers_asm(&_State);
                        _Update_board();
                    }
                    InvalidateRect(_Window_handle, nullptr, FALSE);
                }
            }
            else if (_App_state == 2)
            {
                if (_Space_is_down && !_Space_was_down)
                {
                    _App_state = 0;
                }
                InvalidateRect(_Window_handle, nullptr, FALSE);
            }

            _Space_was_down = _Space_is_down;
            _STD this_thread::sleep_for(2ms);
        }

        DestroyWindow(_Window_handle);
    }

  protected:
    template <class _Self> void _Update_board(this const _Self &_Self_obj) noexcept
    {
        constexpr size_t _Total_bytes = _Width * _Height * sizeof(_Tile_type);
        constexpr size_t _Qwords = _Total_bytes / 8;
        __assume(_Total_bytes % 8 == 0);

        _Clear_board_asm(const_cast<_Tile_type *>(_Self_obj._Board_data), _Qwords);

        const int _Camera_y = _Self_obj._State._Max_y - 20;

        for (int _I = 0; _I < 16; ++_I)
        {
            const int _Sy = _Self_obj._State._Saucers_y[_I] - _Camera_y;
            const int _Sx = _Self_obj._State._Saucers_x[_I];
            if (_Sy >= 0 && _Sy < static_cast<int>(_Height))
            {
                for (int _Dx = -3; _Dx <= 3; ++_Dx)
                {
                    const int _Px = _Sx + _Dx;
                    if (_Px >= 0 && _Px < static_cast<int>(_Width))
                    {
                        const_cast<_Tile_type *>(_Self_obj._Board_data)[_Sy * _Width + _Px] = _Tile_type::_Saucer;
                    }
                }
            }
        }

        const int _Py = _Self_obj._State._Player_y - _Camera_y;
        const int _Px = _Self_obj._State._Player_x;
        if (_Py >= 0 && _Py < static_cast<int>(_Height) && _Px >= 0 && _Px < static_cast<int>(_Width))
        {
            const_cast<_Tile_type *>(_Self_obj._Board_data)[_Py * _Width + _Px] =
                _Self_obj._State._Velocity_y < 0 ? _Tile_type::_Player_Up : _Tile_type::_Player_Down;
        }
    }
};

_STD_END

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    _STD _Jumper_engine<80, 40> _Game_instance;
    _Game_instance._Run();
    return 0;
}

#pragma pop_macro("new")
#pragma pack(pop)
#pragma warning(pop)

#endif

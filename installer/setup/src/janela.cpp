#include <algorithm>
// O gdiplus.h usa min/max: com o MinGW eles precisam estar visíveis no escopo global.
using std::max;
using std::min;

#include <windows.h>

#include <dwmapi.h>
#include <gdiplus.h>
#include <shlwapi.h>

#include "../recursos/recursos.h"
#include "instalar.h"
#include "janela.h"
#include "sistema.h"

#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

using namespace Gdiplus;

namespace Janela {

namespace {

constexpr float kLargura = 760.f;
constexpr float kAltura = 530.f;
constexpr UINT WM_TERMINOU = WM_APP + 1;
constexpr UINT_PTR kTemporizador = 1;

// Cores do tema escuro do Caderno+ (espelho de src/core/Tokens.h / design/tokens.json).
const Color kFundoTopo(255, 0x14, 0x1c, 0x1a);
const Color kFundoBase(255, 0x0a, 0x0f, 0x0e);
const Color kTinta(255, 0xee, 0xf0, 0xec);
const Color kTintaSuave(255, 0xa7, 0xb0, 0xaa);
const Color kPrimaria(255, 0x6f, 0xbf, 0xa5);
const Color kPrimariaClara(255, 0xa4, 0xe3, 0xcd);
const Color kSobrePrimaria(255, 0x0f, 0x1f, 0x1a);
const Color kDestaque(255, 0xe0, 0xa6, 0x4b);
const Color kPerigo(255, 0xf0, 0x8a, 0x80);
const Color kLinha(255, 0x36, 0x40, 0x3c);

Color comAlfa(const Color &c, float alfa)
{
    return Color(static_cast<BYTE>((std::max)(0.f, (std::min)(255.f, alfa * 255.f))), c.GetR(), c.GetG(), c.GetB());
}

float limitar(float v, float a, float b)
{
    return (std::max)(a, (std::min)(b, v));
}

float suavizar(float t)  // easeOutCubic
{
    t = limitar(t, 0.f, 1.f);
    return 1.f - std::pow(1.f - t, 3.f);
}

double agora()
{
    return static_cast<double>(GetTickCount64()) / 1000.0;
}

void caminhoRedondo(GraphicsPath &p, const RectF &r, float raio)
{
    raio = (std::min)(raio, (std::min)(r.Width, r.Height) / 2.f);
    const float d = raio * 2.f;
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
    p.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
    p.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
    p.CloseFigure();
}

void preencherRedondo(Graphics &g, const Brush &pincel, const RectF &r, float raio)
{
    GraphicsPath p;
    caminhoRedondo(p, r, raio);
    g.FillPath(&pincel, &p);
}

void contornoRedondo(Graphics &g, const Pen &caneta, const RectF &r, float raio)
{
    GraphicsPath p;
    caminhoRedondo(p, r, raio);
    g.DrawPath(&caneta, &p);
}

// Brilho suave (mancha de luz) centrado em (cx, cy).
void brilho(Graphics &g, float cx, float cy, float raio, const Color &cor, float intensidade)
{
    GraphicsPath p;
    p.AddEllipse(cx - raio, cy - raio, raio * 2, raio * 2);
    PathGradientBrush pincel(&p);
    pincel.SetCenterColor(comAlfa(cor, intensidade));
    Color borda = comAlfa(cor, 0.f);
    int n = 1;
    pincel.SetSurroundColors(&borda, &n);
    g.FillPath(&pincel, &p);
}

enum class Estilo { Primario, Secundario, Perigo, Link };

struct Controle {
    RectF r;
    std::wstring texto;
    Estilo estilo = Estilo::Secundario;
    bool caixa = false;        // caixa de seleção?
    bool *marca = nullptr;     // estado da caixa
    bool ativo = true;
    std::function<void()> acao;
    float hover = 0.f;
    bool pressionado = false;
};

enum class Pagina { BoasVindas, Instalando, Concluido, Erro, Confirmar, Removendo, Removido };

class Tela {
public:
    Tela(HINSTANCE instancia, const Configuracao &config);
    ~Tela();

    bool criarJanela(int mostrar);
    int laco();

    // Desenho (independe da janela: também usado para gerar as imagens de conferência).
    void desenhar(Graphics &g, double t);
    void definirPagina(Pagina p);
    void simularAndamento(int milesimos, const std::wstring &arquivo) { simulado_ = true; milesimosSimulados_ = milesimos; arquivoSimulado_ = arquivo; progressoSuave_ = milesimos / 1000.f; }
    void definirErroSimulado(const std::wstring &mensagem) { erro_ = mensagem; }
    float escala() const { return escala_; }

private:
    static LRESULT CALLBACK procedimento(HWND, UINT, WPARAM, LPARAM);
    LRESULT tratar(UINT, WPARAM, LPARAM);

    void carregarRecursos();
    void construirControles();
    Controle *sob(PointF p);
    PointF paraLogico(LPARAM lp) const;
    void atualizarAnimacoes(double dt);
    void atualizarEscala();
    void pintar();
    void ativar(Controle &c);
    void mover(int sentido);

    void iniciarInstalacao();
    void iniciarRemocao();
    void alterarPasta();
    void abrirAplicativoEFechar();
    float progresso() const;
    std::wstring arquivoAtual();

    // Desenho por partes
    void fundo(Graphics &g, double t);
    void logo(Graphics &g, float cx, float cy, float tamanho, double t);
    void titulo(Graphics &g, const std::wstring &texto, float y, float alfa, float desloc);
    void subtitulo(Graphics &g, const std::wstring &texto, float y, float alfa, float desloc, float altura = 52.f);
    void controle(Graphics &g, const Controle &c, bool comFoco, float alfa, float desloc);
    void barraDeProgresso(Graphics &g, float x, float y, float largura, float valor, double t);
    void marcaDeConcluido(Graphics &g, float cx, float cy, float t, const Color &cor, bool erro);
    void botoesDaJanela(Graphics &g);
    void cartaoDeOpcoes(Graphics &g, float alfa, float desloc);

    float largura(Graphics &g, const std::wstring &s, const Font &f);
    void texto(Graphics &g, const std::wstring &s, const Font &f, RectF r, const Color &c, StringAlignment h = StringAlignmentNear,
               StringAlignment v = StringAlignmentNear, bool umaLinha = false, bool caminho = false);

    HINSTANCE instancia_;
    Configuracao config_;
    HWND janela_ = nullptr;
    float escala_ = 1.f;

    // Recursos visuais
    PrivateFontCollection colecao_;
    std::unique_ptr<FontFamily> familia_;
    std::unique_ptr<Font> fTitulo_, fSubtitulo_, fBotao_, fPequena_, fNegrito_, fMarca_;
    std::unique_ptr<Image> logo_;
    IStream *fluxoDoLogo_ = nullptr;
    std::unique_ptr<Bitmap> buffer_;

    // Estado
    Pagina pagina_ = Pagina::BoasVindas;
    double inicioDaPagina_ = 0;
    double ultimoQuadro_ = 0;
    std::vector<Controle> controles_;
    int foco_ = -1;
    PointF mouse_{-1, -1};
    bool rastreando_ = false;
    bool teclado_ = false;  // o anel de foco só aparece depois que a pessoa usa o teclado

    // Opções
    std::wstring pasta_;
    bool atalho_ = true;
    bool apagarDados_ = false;
    std::wstring erro_;
    bool simulado_ = false;
    int milesimosSimulados_ = 0;
    std::wstring arquivoSimulado_;
    float progressoSuave_ = 0.f;

    // Trabalho em segundo plano
    std::thread trabalho_;
    Instalar::Andamento andamento_;
    bool ocupado_ = false;
    int codigoDeSaida_ = 0;
};

Tela::Tela(HINSTANCE instancia, const Configuracao &config) : instancia_(instancia), config_(config)
{
    pasta_ = config.pasta;
    // O GDI+ é iniciado uma única vez por processo e fica até o fim: reiniciá-lo deixa "ponteiros fantasmas"
    // (como o de StringFormat::GenericTypographic) e causava falha ao encerrar.
    static const bool gdiIniciado = [] {
        static ULONG_PTR token = 0;
        GdiplusStartupInput entrada;
        return GdiplusStartup(&token, &entrada, nullptr) == Ok;
    }();
    (void)gdiIniciado;
    carregarRecursos();
}

Tela::~Tela()
{
    if (trabalho_.joinable())
        trabalho_.join();
    buffer_.reset();
    logo_.reset();
    if (fluxoDoLogo_)
        fluxoDoLogo_->Release();
    fTitulo_.reset();
    fSubtitulo_.reset();
    fBotao_.reset();
    fPequena_.reset();
    fNegrito_.reset();
    fMarca_.reset();
    familia_.reset();
}

void Tela::carregarRecursos()
{
    auto recurso = [&](int id, const wchar_t *tipo, const void **dados, DWORD *tamanho) {
        HRSRC r = FindResourceW(instancia_, MAKEINTRESOURCEW(id), tipo);
        if (!r)
            return false;
        HGLOBAL h = LoadResource(instancia_, r);
        if (!h)
            return false;
        *dados = LockResource(h);
        *tamanho = SizeofResource(instancia_, r);
        return *dados != nullptr;
    };

    const void *dados = nullptr;
    DWORD tamanho = 0;
    if (recurso(IDR_FONTE_REGULAR, RT_RCDATA, &dados, &tamanho))
        colecao_.AddMemoryFont(dados, static_cast<INT>(tamanho));
    if (recurso(IDR_FONTE_NEGRITO, RT_RCDATA, &dados, &tamanho))
        colecao_.AddMemoryFont(dados, static_cast<INT>(tamanho));
    const INT quantidade = colecao_.GetFamilyCount();
    if (quantidade > 0) {
        std::vector<FontFamily> familias(static_cast<size_t>(quantidade));
        INT achadas = 0;
        colecao_.GetFamilies(quantidade, familias.data(), &achadas);
        if (achadas > 0)
            familia_.reset(familias[0].Clone());
    }
    if (!familia_)
        familia_ = std::make_unique<FontFamily>(L"Segoe UI");

    fTitulo_ = std::make_unique<Font>(familia_.get(), 36.f, FontStyleBold, UnitPixel);
    fSubtitulo_ = std::make_unique<Font>(familia_.get(), 16.f, FontStyleRegular, UnitPixel);
    fBotao_ = std::make_unique<Font>(familia_.get(), 17.f, FontStyleBold, UnitPixel);
    fPequena_ = std::make_unique<Font>(familia_.get(), 13.f, FontStyleRegular, UnitPixel);
    fNegrito_ = std::make_unique<Font>(familia_.get(), 14.f, FontStyleBold, UnitPixel);
    fMarca_ = std::make_unique<Font>(familia_.get(), 30.f, FontStyleBold, UnitPixel);

    if (recurso(IDR_LOGO, RT_RCDATA, &dados, &tamanho)) {
        fluxoDoLogo_ = SHCreateMemStream(static_cast<const BYTE *>(dados), tamanho);
        if (fluxoDoLogo_)
            logo_.reset(Image::FromStream(fluxoDoLogo_));
    }
}

// ------------------------------------------------------------------ janela

bool Tela::criarJanela(int mostrar)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = procedimento;
    wc.hInstance = instancia_;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instancia_, MAKEINTRESOURCEW(IDI_ICONE));
    wc.hIconSm = wc.hIcon;
    wc.lpszClassName = L"CadernoSetupJanela";
    RegisterClassExW(&wc);

    // Tamanho inicial em pixels (a escala só é conhecida depois que a janela existe).
    HDC tela = GetDC(nullptr);
    const int dpi = GetDeviceCaps(tela, LOGPIXELSX);
    ReleaseDC(nullptr, tela);
    const float e = dpi / 96.f;
    const int w = static_cast<int>(kLargura * e), h = static_cast<int>(kAltura * e);
    const int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2, y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    janela_ = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, config_.modo == Modo::Instalar ? L"Instalador do Caderno+" : L"Desinstalar o Caderno+",
                              WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU, x, y, w, h, nullptr, nullptr, instancia_, this);
    if (!janela_)
        return false;

    // Sombra e cantos arredondados (Windows 11); no Windows 10 a janela fica com cantos retos, com sombra.
    const MARGINS margens = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(janela_, &margens);
    const DWORD arredondar = 2;  // DWMWCP_ROUND
    DwmSetWindowAttribute(janela_, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &arredondar, sizeof arredondar);
    atualizarEscala();

    definirPagina(config_.modo == Modo::Instalar ? Pagina::BoasVindas : Pagina::Confirmar);
    ShowWindow(janela_, mostrar);
    SetTimer(janela_, kTemporizador, 16, nullptr);
    return true;
}

int Tela::laco()
{
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return codigoDeSaida_;
}

LRESULT CALLBACK Tela::procedimento(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    Tela *tela = nullptr;
    if (msg == WM_NCCREATE) {
        tela = static_cast<Tela *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(tela));
        tela->janela_ = hwnd;
    } else {
        tela = reinterpret_cast<Tela *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (!tela)
        return DefWindowProcW(hwnd, msg, wp, lp);
    return tela->tratar(msg, wp, lp);
}

void Tela::atualizarEscala()
{
    UINT dpi = 96;
    using ObterDpi = UINT(WINAPI *)(HWND);
    if (HMODULE usuario = GetModuleHandleW(L"user32.dll")) {
        if (auto f = reinterpret_cast<ObterDpi>(GetProcAddress(usuario, "GetDpiForWindow")))
            dpi = f(janela_);
    }
    escala_ = dpi / 96.f;
    const int w = static_cast<int>(kLargura * escala_), h = static_cast<int>(kAltura * escala_);
    RECT r;
    GetWindowRect(janela_, &r);
    if (r.right - r.left != w || r.bottom - r.top != h)
        SetWindowPos(janela_, nullptr, r.left, r.top, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    buffer_.reset(new Bitmap(w, h, PixelFormat32bppPARGB));
}

PointF Tela::paraLogico(LPARAM lp) const
{
    return PointF(static_cast<float>(static_cast<short>(LOWORD(lp))) / escala_, static_cast<float>(static_cast<short>(HIWORD(lp))) / escala_);
}

LRESULT Tela::tratar(UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        pintar();
        return 0;
    case WM_TIMER: {
        const double t = agora();
        atualizarAnimacoes(t - ultimoQuadro_);
        ultimoQuadro_ = t;
        InvalidateRect(janela_, nullptr, FALSE);
        return 0;
    }
    case WM_DPICHANGED: {
        const RECT *sugerido = reinterpret_cast<const RECT *>(lp);
        SetWindowPos(janela_, nullptr, sugerido->left, sugerido->top, sugerido->right - sugerido->left, sugerido->bottom - sugerido->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        atualizarEscala();
        return 0;
    }
    case WM_NCHITTEST: {
        POINT p = {static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp))};
        ScreenToClient(janela_, &p);
        const float x = p.x / escala_, y = p.y / escala_;
        if (y >= 0 && y < 56 && x < kLargura - 100)
            return HTCAPTION;  // arrasta a janela pela faixa de cima
        return HTCLIENT;
    }
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(janela_, &p);
            Controle *c = sob(PointF(p.x / escala_, p.y / escala_));
            const bool botaoDaJanela = p.y / escala_ < 56 && p.x / escala_ >= kLargura - 100;
            SetCursor(LoadCursorW(nullptr, (c && c->ativo) || botaoDaJanela ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_MOUSEMOVE: {
        mouse_ = paraLogico(lp);
        if (!rastreando_) {
            TRACKMOUSEEVENT te = {sizeof te, TME_LEAVE, janela_, 0};
            TrackMouseEvent(&te);
            rastreando_ = true;
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        rastreando_ = false;
        mouse_ = PointF(-1, -1);
        for (Controle &c : controles_)
            c.pressionado = false;
        return 0;
    case WM_LBUTTONDOWN: {
        const PointF p = paraLogico(lp);
        mouse_ = p;
        teclado_ = false;
        // Botões da janela (minimizar / fechar), desenhados no canto.
        if (p.Y >= 12 && p.Y <= 44) {
            if (p.X >= kLargura - 52 && p.X <= kLargura - 14) {
                if (!ocupado_)
                    PostMessageW(janela_, WM_CLOSE, 0, 0);
                return 0;
            }
            if (p.X >= kLargura - 94 && p.X <= kLargura - 56) {
                ShowWindow(janela_, SW_MINIMIZE);
                return 0;
            }
        }
        if (Controle *c = sob(p)) {
            if (c->ativo) {
                c->pressionado = true;
                SetCapture(janela_);
                foco_ = static_cast<int>(c - controles_.data());
            }
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        ReleaseCapture();
        const PointF p = paraLogico(lp);
        Controle *c = sob(p);
        bool dispara = false;
        for (Controle &x : controles_) {
            if (x.pressionado && &x == c)
                dispara = true;
            x.pressionado = false;
        }
        if (dispara && c)
            ativar(*c);
        return 0;
    }
    case WM_KEYDOWN:
        teclado_ = true;
        if (wp == VK_ESCAPE) {
            if (!ocupado_)
                PostMessageW(janela_, WM_CLOSE, 0, 0);
        } else if (wp == VK_TAB) {
            mover((GetKeyState(VK_SHIFT) & 0x8000) ? -1 : 1);
        } else if (wp == VK_LEFT || wp == VK_UP) {
            mover(-1);
        } else if (wp == VK_RIGHT || wp == VK_DOWN) {
            mover(1);
        } else if (wp == VK_RETURN || wp == VK_SPACE) {
            if (foco_ >= 0 && foco_ < static_cast<int>(controles_.size()) && controles_[foco_].ativo)
                ativar(controles_[foco_]);
        }
        return 0;
    case WM_CLOSE:
        if (ocupado_)
            return 0;  // não fecha no meio da instalação
        DestroyWindow(janela_);
        return 0;
    case WM_TERMINOU:
        ocupado_ = false;
        if (trabalho_.joinable())
            trabalho_.join();
        if (andamento_.ok) {
            codigoDeSaida_ = 0;
            definirPagina(config_.modo == Modo::Instalar ? Pagina::Concluido : Pagina::Removido);
        } else {
            erro_ = andamento_.mensagemDeErro();
            codigoDeSaida_ = 4;
            definirPagina(Pagina::Erro);
        }
        return 0;
    case WM_DESTROY:
        KillTimer(janela_, kTemporizador);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(janela_, msg, wp, lp);
}

void Tela::pintar()
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(janela_, &ps);
    if (buffer_) {
        {
            Graphics g(buffer_.get());
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintAntiAlias);
            g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            g.ResetTransform();
            g.ScaleTransform(escala_, escala_);
            desenhar(g, agora());
        }
        Graphics janela(hdc);
        janela.DrawImage(buffer_.get(), 0, 0, static_cast<INT>(buffer_->GetWidth()), static_cast<INT>(buffer_->GetHeight()));
    }
    EndPaint(janela_, &ps);
}

// --------------------------------------------------------------- interação

Controle *Tela::sob(PointF p)
{
    for (Controle &c : controles_)
        if (c.r.Contains(p))
            return &c;
    return nullptr;
}

void Tela::mover(int sentido)
{
    if (controles_.empty())
        return;
    const int n = static_cast<int>(controles_.size());
    int i = foco_;
    for (int tentativas = 0; tentativas < n; ++tentativas) {
        i = ((i < 0 ? (sentido > 0 ? -1 : 0) : i) + sentido + n) % n;
        if (controles_[i].ativo) {
            foco_ = i;
            return;
        }
    }
}

void Tela::atualizarAnimacoes(double dt)
{
    dt = limitar(static_cast<float>(dt), 0.f, 0.1f);
    for (Controle &c : controles_) {
        const bool sobre = c.ativo && c.r.Contains(mouse_);
        const float alvo = c.pressionado ? 1.f : (sobre ? 0.75f : 0.f);
        c.hover += (alvo - c.hover) * limitar(static_cast<float>(dt) * 14.f, 0.f, 1.f);
    }
    // A barra acompanha o andamento real com suavidade (sem "pulos").
    const float meta = progresso();
    progressoSuave_ += (meta - progressoSuave_) * limitar(static_cast<float>(dt) * 6.f, 0.f, 1.f);
    if (std::fabs(meta - progressoSuave_) < 0.002f)
        progressoSuave_ = meta;
}

float Tela::progresso() const
{
    if (simulado_)
        return milesimosSimulados_ / 1000.f;
    return andamento_.milesimos.load() / 1000.f;
}

std::wstring Tela::arquivoAtual()
{
    return simulado_ ? arquivoSimulado_ : andamento_.arquivo();
}

void Tela::ativar(Controle &c)
{
    if (c.caixa && c.marca)
        *c.marca = !*c.marca;
    if (c.acao)
        c.acao();
}

void Tela::definirPagina(Pagina p)
{
    pagina_ = p;
    inicioDaPagina_ = agora();
    construirControles();
    foco_ = -1;
    for (size_t i = 0; i < controles_.size() && foco_ < 0; ++i)
        if (controles_[i].ativo && (controles_[i].estilo == Estilo::Primario || controles_[i].estilo == Estilo::Perigo))
            foco_ = static_cast<int>(i);
    for (size_t i = 0; i < controles_.size() && foco_ < 0; ++i)
        if (controles_[i].ativo)
            foco_ = static_cast<int>(i);
}

void Tela::construirControles()
{
    controles_.clear();
    auto botao = [&](float cx, float y, float w, float h, const std::wstring &texto, Estilo estilo, std::function<void()> acao) {
        Controle c;
        c.r = RectF(cx - w / 2, y, w, h);
        c.texto = texto;
        c.estilo = estilo;
        c.acao = std::move(acao);
        controles_.push_back(std::move(c));
    };
    const float cx = kLargura / 2;

    switch (pagina_) {
    case Pagina::BoasVindas: {
        // Caixa "atalho" e botão "Alterar…" dentro do cartão de opções (veja cartaoDeOpcoes).
        Controle alterar;
        alterar.r = RectF(kLargura / 2 + 280 - 20 - 100, 318, 100, 34);
        alterar.texto = L"Alterar…";
        alterar.estilo = Estilo::Secundario;
        alterar.acao = [this] { alterarPasta(); };
        controles_.push_back(alterar);
        Controle caixa;
        caixa.r = RectF(kLargura / 2 - 280 + 20, 366, 330, 24);
        caixa.texto = L"Criar um atalho na área de trabalho";
        caixa.caixa = true;
        caixa.marca = &atalho_;
        controles_.push_back(caixa);
        botao(cx, 434, 270, 54, config_.jaInstalado ? L"Atualizar" : L"Instalar", Estilo::Primario, [this] { iniciarInstalacao(); });
        break;
    }
    case Pagina::Concluido:
        botao(cx, 392, 270, 54, L"Abrir o Caderno+", Estilo::Primario, [this] { abrirAplicativoEFechar(); });
        botao(cx, 456, 150, 38, L"Fechar", Estilo::Link, [this] { PostMessageW(janela_, WM_CLOSE, 0, 0); });
        break;
    case Pagina::Erro:
        botao(cx - 90, 430, 170, 50, L"Tentar de novo", Estilo::Primario, [this] { definirPagina(config_.modo == Modo::Instalar ? Pagina::BoasVindas : Pagina::Confirmar); });
        botao(cx + 100, 430, 150, 50, L"Fechar", Estilo::Secundario, [this] { PostMessageW(janela_, WM_CLOSE, 0, 0); });
        break;
    case Pagina::Confirmar: {
        Controle caixa;
        caixa.r = RectF(cx - 280, 322, 560, 24);
        caixa.texto = L"Apagar também os meus dados (turmas, notas, contas e configurações)";
        caixa.caixa = true;
        caixa.marca = &apagarDados_;
        controles_.push_back(caixa);
        botao(cx - 100, 428, 190, 54, L"Remover", Estilo::Perigo, [this] { iniciarRemocao(); });
        botao(cx + 110, 428, 150, 54, L"Cancelar", Estilo::Secundario, [this] { PostMessageW(janela_, WM_CLOSE, 0, 0); });
        break;
    }
    case Pagina::Removido:
        botao(cx, 428, 200, 54, L"Fechar", Estilo::Primario, [this] { PostMessageW(janela_, WM_CLOSE, 0, 0); });
        break;
    case Pagina::Instalando:
    case Pagina::Removendo:
        break;
    }
}

void Tela::alterarPasta()
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const std::wstring escolhida = Sistema::escolherPasta(janela_, L"Escolha a pasta de instalação", Sistema::existe(pasta_) ? pasta_ : Sistema::pastaDoArquivo(pasta_));
    if (!escolhida.empty())
        pasta_ = Instalar::resolverPasta(escolhida);
}

void Tela::iniciarInstalacao()
{
    const std::wstring problema = Instalar::validarPasta(pasta_);
    if (!problema.empty()) {
        erro_ = problema;
        definirPagina(Pagina::Erro);
        return;
    }
    Instalar::Opcoes opcoes;
    opcoes.pasta = Instalar::resolverPasta(pasta_);
    opcoes.atalhoAreaDeTrabalho = atalho_;
    pasta_ = opcoes.pasta;
    andamento_.milesimos = 0;
    andamento_.terminou = false;
    andamento_.ok = false;
    andamento_.definirArquivo(L"");
    andamento_.definirErro(L"");
    progressoSuave_ = 0.f;
    ocupado_ = true;
    definirPagina(Pagina::Instalando);
    HWND hwnd = janela_;
    trabalho_ = std::thread([this, opcoes, hwnd] {
        Instalar::instalar(config_.pacote, opcoes, andamento_);
        PostMessageW(hwnd, WM_TERMINOU, 0, 0);
    });
}

void Tela::iniciarRemocao()
{
    if (apagarDados_) {
        const int r = MessageBoxW(janela_,
                                  L"Isso apaga PARA SEMPRE as turmas, alunos, notas, frequência, anotações, contas e configurações deste computador.\n\n"
                                  L"Se quiser guardar uma cópia, cancele e faça um backup antes (Backup no Caderno+).\n\nApagar tudo mesmo?",
                                  L"Apagar os dados?", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        if (r != IDYES)
            return;
    }
    andamento_.milesimos = 0;
    andamento_.terminou = false;
    andamento_.ok = false;
    andamento_.definirArquivo(L"");
    andamento_.definirErro(L"");
    progressoSuave_ = 0.f;
    ocupado_ = true;
    definirPagina(Pagina::Removendo);
    HWND hwnd = janela_;
    const std::wstring pasta = pasta_;
    const bool dados = apagarDados_;
    trabalho_ = std::thread([this, pasta, dados, hwnd] {
        Instalar::desinstalar(pasta, dados, andamento_);
        PostMessageW(hwnd, WM_TERMINOU, 0, 0);
    });
}

void Tela::abrirAplicativoEFechar()
{
    const std::wstring exe = Sistema::juntar(pasta_, Sistema::paraLargo(config_.pacote.valor("exe", "ProfOrganizer.exe")));
    Sistema::executar(exe, L"", pasta_, false);
    PostMessageW(janela_, WM_CLOSE, 0, 0);
}

// ------------------------------------------------------------------ desenho

void Tela::texto(Graphics &g, const std::wstring &s, const Font &f, RectF r, const Color &c, StringAlignment h, StringAlignment v, bool umaLinha, bool caminho)
{
    StringFormat sf;
    sf.SetAlignment(h);
    sf.SetLineAlignment(v);
    sf.SetTrimming(caminho ? StringTrimmingEllipsisPath : StringTrimmingEllipsisCharacter);
    if (umaLinha)
        sf.SetFormatFlags(StringFormatFlagsNoWrap);
    SolidBrush pincel(c);
    g.DrawString(s.c_str(), -1, &f, r, &sf, &pincel);
}

void Tela::fundo(Graphics &g, double t)
{
    LinearGradientBrush base(PointF(0, 0), PointF(0, kAltura), kFundoTopo, kFundoBase);
    g.FillRectangle(&base, RectF(0, 0, kLargura, kAltura));
    const float ft = static_cast<float>(t);
    brilho(g, 130 + std::sin(ft * 0.45f) * 40, 70 + std::cos(ft * 0.35f) * 24, 430, kPrimaria, 0.20f);
    brilho(g, 650 + std::cos(ft * 0.40f) * 36, 470 + std::sin(ft * 0.30f) * 26, 380, kDestaque, 0.13f);
    brilho(g, 380, 250, 300, kPrimaria, 0.05f);
    Pen borda(comAlfa(kTinta, 0.09f), 1.f);
    g.DrawRectangle(&borda, RectF(0.5f, 0.5f, kLargura - 1.f, kAltura - 1.f));
}

void Tela::logo(Graphics &g, float cx, float cy, float tamanho, double t)
{
    const float respira = 1.f + 0.035f * std::sin(static_cast<float>(t) * 1.6f);
    brilho(g, cx, cy, tamanho * 1.2f * respira, kPrimaria, 0.30f);
    brilho(g, cx + tamanho * 0.35f, cy + tamanho * 0.30f, tamanho * 0.7f, kDestaque, 0.16f);
    if (logo_ && logo_->GetLastStatus() == Ok) {
        // Sombra suave sob o ícone e o ícone em si.
        SolidBrush sombra(Color(70, 0, 0, 0));
        preencherRedondo(g, sombra, RectF(cx - tamanho / 2 + 2, cy - tamanho / 2 + 8, tamanho, tamanho), tamanho * 0.22f);
        g.DrawImage(logo_.get(), RectF(cx - tamanho / 2, cy - tamanho / 2, tamanho, tamanho));
    }
}

float Tela::largura(Graphics &g, const std::wstring &s, const Font &f)
{
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces | StringFormatFlagsNoWrap);
    RectF caixa;
    g.MeasureString(s.c_str(), -1, &f, PointF(0, 0), &sf, &caixa);
    return caixa.Width;
}

// Título centralizado; cada "+" sai em ocre (a assinatura do Caderno+).
void Tela::titulo(Graphics &g, const std::wstring &s, float y, float alfa, float desloc)
{
    std::vector<std::wstring> partes;
    std::wstring atual;
    for (wchar_t c : s) {
        if (c == L'+') {
            if (!atual.empty())
                partes.push_back(atual);
            partes.push_back(L"+");
            atual.clear();
        } else {
            atual.push_back(c);
        }
    }
    if (!atual.empty())
        partes.push_back(atual);

    float total = 0.f;
    std::vector<float> larguras;
    for (const std::wstring &p : partes) {
        larguras.push_back(largura(g, p, *fTitulo_));
        total += larguras.back();
    }
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces | StringFormatFlagsNoWrap);
    float x = (kLargura - total) / 2.f;
    for (size_t i = 0; i < partes.size(); ++i) {
        SolidBrush pincel(partes[i] == L"+" ? comAlfa(kDestaque, alfa) : comAlfa(kTinta, alfa));
        g.DrawString(partes[i].c_str(), -1, fTitulo_.get(), PointF(x, y + desloc + 2.f), &sf, &pincel);
        x += larguras[i];
    }
}

void Tela::subtitulo(Graphics &g, const std::wstring &s, float y, float alfa, float desloc, float altura)
{
    texto(g, s, *fSubtitulo_, RectF(110, y + desloc, kLargura - 220, altura), comAlfa(kTintaSuave, alfa), StringAlignmentCenter, StringAlignmentNear);
}

void Tela::botoesDaJanela(Graphics &g)
{
    auto desenhar = [&](float x, bool fechar) {
        const RectF r(x, 12.f, 38.f, 32.f);
        const bool sobre = r.Contains(mouse_) && !(fechar && ocupado_);
        if (sobre) {
            SolidBrush fundoHover(fechar ? comAlfa(kPerigo, 0.9f) : comAlfa(kTinta, 0.12f));
            preencherRedondo(g, fundoHover, r, 8);
        }
        Pen caneta(fechar && ocupado_ ? comAlfa(kTintaSuave, 0.35f) : (sobre && fechar ? kSobrePrimaria : kTintaSuave), 1.6f);
        caneta.SetStartCap(LineCapRound);
        caneta.SetEndCap(LineCapRound);
        const float cx = r.X + r.Width / 2, cy = r.Y + r.Height / 2;
        if (fechar) {
            g.DrawLine(&caneta, cx - 5, cy - 5, cx + 5, cy + 5);
            g.DrawLine(&caneta, cx + 5, cy - 5, cx - 5, cy + 5);
        } else {
            g.DrawLine(&caneta, cx - 5, cy + 1, cx + 5, cy + 1);
        }
    };
    desenhar(kLargura - 94, false);
    desenhar(kLargura - 52, true);
}

void Tela::cartaoDeOpcoes(Graphics &g, float alfa, float desloc)
{
    const RectF cartao(kLargura / 2 - 280, 296.f + desloc, 560, 112);
    SolidBrush fundoCartao(comAlfa(kTinta, 0.05f * alfa));
    preencherRedondo(g, fundoCartao, cartao, 16);
    Pen borda(comAlfa(kTinta, 0.10f * alfa), 1.f);
    contornoRedondo(g, borda, cartao, 16);
    texto(g, L"Instalar em", *fPequena_, RectF(cartao.X + 20, cartao.Y + 14, 200, 18), comAlfa(kTintaSuave, alfa));
    texto(g, pasta_, *fNegrito_, RectF(cartao.X + 20, cartao.Y + 34, 400, 22), comAlfa(kTinta, alfa), StringAlignmentNear, StringAlignmentCenter, true, true);
}

void Tela::controle(Graphics &g, const Controle &c, bool comFoco, float alfa, float desloc)
{
    RectF r = c.r;
    r.Y += desloc;
    const float h = c.hover;
    if (c.caixa) {
        const RectF quadro(r.X, r.Y + 2, 20, 20);
        const bool marcada = c.marca && *c.marca;
        if (marcada) {
            LinearGradientBrush pincel(quadro, kPrimariaClara, kPrimaria, 90.f);
            preencherRedondo(g, pincel, quadro, 6);
            Pen visto(comAlfa(kSobrePrimaria, alfa), 2.2f);
            visto.SetStartCap(LineCapRound);
            visto.SetEndCap(LineCapRound);
            visto.SetLineJoin(LineJoinRound);
            const PointF pontos[3] = {PointF(quadro.X + 5, quadro.Y + 10.5f), PointF(quadro.X + 8.5f, quadro.Y + 14), PointF(quadro.X + 15, quadro.Y + 6.5f)};
            g.DrawLines(&visto, pontos, 3);
        } else {
            SolidBrush fundoCaixa(comAlfa(kTinta, (0.05f + 0.07f * h) * alfa));
            preencherRedondo(g, fundoCaixa, quadro, 6);
            Pen contorno(comAlfa(kTintaSuave, (0.55f + 0.3f * h) * alfa), 1.4f);
            contornoRedondo(g, contorno, quadro, 6);
        }
        texto(g, c.texto, *fSubtitulo_, RectF(r.X + 32, r.Y, r.Width - 32, 24), comAlfa(kTinta, alfa), StringAlignmentNear, StringAlignmentCenter, true);
        if (comFoco) {
            Pen anel(comAlfa(kPrimariaClara, alfa), 2.f);
            contornoRedondo(g, anel, RectF(quadro.X - 3, quadro.Y - 3, quadro.Width + 6, quadro.Height + 6), 8);
        }
        return;
    }

    const float raio = (std::min)(r.Height / 2.f, 27.f);
    switch (c.estilo) {
    case Estilo::Primario:
    case Estilo::Perigo: {
        const Color cor = c.estilo == Estilo::Primario ? kPrimaria : kPerigo;
        const Color clara = c.estilo == Estilo::Primario ? kPrimariaClara : Color(255, 0xff, 0xb4, 0xab);
        brilho(g, r.X + r.Width / 2, r.Y + r.Height / 2 + 6, r.Width * 0.75f, cor, (0.16f + 0.30f * h) * alfa);
        LinearGradientBrush pincel(r, comAlfa(h > 0.4f ? clara : cor, alfa), comAlfa(cor, alfa), 90.f);
        preencherRedondo(g, pincel, r, raio);
        SolidBrush luz(comAlfa(Color(255, 255, 255, 255), 0.10f * h * alfa));
        preencherRedondo(g, luz, r, raio);
        const float afundar = c.pressionado ? 1.f : 0.f;
        texto(g, c.texto, *fBotao_, RectF(r.X, r.Y + afundar, r.Width, r.Height), comAlfa(kSobrePrimaria, alfa), StringAlignmentCenter, StringAlignmentCenter, true);
        break;
    }
    case Estilo::Secundario: {
        SolidBrush fundoBotao(comAlfa(kTinta, (0.07f + 0.08f * h) * alfa));
        preencherRedondo(g, fundoBotao, r, raio);
        Pen contorno(comAlfa(kTinta, (0.14f + 0.18f * h) * alfa), 1.f);
        contornoRedondo(g, contorno, r, raio);
        texto(g, c.texto, c.r.Height < 40 ? *fNegrito_ : *fBotao_, r, comAlfa(kTinta, alfa), StringAlignmentCenter, StringAlignmentCenter, true);
        break;
    }
    case Estilo::Link: {
        texto(g, c.texto, *fNegrito_, r, comAlfa(h > 0.3f ? kTinta : kTintaSuave, alfa), StringAlignmentCenter, StringAlignmentCenter, true);
        if (h > 0.05f) {
            Pen sublinhado(comAlfa(kTinta, h * alfa), 1.2f);
            g.DrawLine(&sublinhado, r.X + r.Width / 2 - 24, r.Y + r.Height / 2 + 11, r.X + r.Width / 2 + 24, r.Y + r.Height / 2 + 11);
        }
        break;
    }
    }
    if (comFoco) {
        Pen anel(comAlfa(kPrimariaClara, alfa), 2.f);
        contornoRedondo(g, anel, RectF(r.X - 4, r.Y - 4, r.Width + 8, r.Height + 8), raio + 4);
    }
}

void Tela::barraDeProgresso(Graphics &g, float x, float y, float largura, float valor, double t)
{
    const float h = 12.f;
    SolidBrush trilho(comAlfa(kTinta, 0.09f));
    preencherRedondo(g, trilho, RectF(x, y, largura, h), h / 2);
    valor = limitar(valor, 0.f, 1.f);
    const float preenchido = (std::max)(h, largura * valor);
    if (valor > 0.001f) {
        brilho(g, x + preenchido, y + h / 2, 70, kPrimaria, 0.35f);
        LinearGradientBrush pincel(RectF(x, y, preenchido, h), kPrimaria, kPrimariaClara, LinearGradientModeHorizontal);
        const RectF barra(x, y, preenchido, h);
        preencherRedondo(g, pincel, barra, h / 2);
        // Faixa de brilho que corre pela barra.
        GraphicsPath caminho;
        caminhoRedondo(caminho, barra, h / 2);
        GraphicsState estado = g.Save();
        g.SetClip(&caminho);
        const float volta = static_cast<float>(std::fmod(t, 1.5) / 1.5);
        const float bx = x - 60 + (preenchido + 120) * volta;
        LinearGradientBrush faixa(PointF(bx, 0), PointF(bx + 60, 0), comAlfa(Color(255, 255, 255, 255), 0.f), comAlfa(Color(255, 255, 255, 255), 0.45f));
        g.FillRectangle(&faixa, RectF(bx, y, 60, h));
        g.Restore(estado);
    }
}

void Tela::marcaDeConcluido(Graphics &g, float cx, float cy, float t, const Color &cor, bool erro)
{
    const float raio = 46.f;
    brilho(g, cx, cy, raio * 2.3f, cor, 0.30f * suavizar(t));
    Pen anel(comAlfa(cor, 0.95f), 4.f);
    anel.SetStartCap(LineCapRound);
    anel.SetEndCap(LineCapRound);
    const float arco = 360.f * suavizar(t / 0.6f);
    if (arco > 0.5f)
        g.DrawArc(&anel, cx - raio, cy - raio, raio * 2, raio * 2, -90.f, arco);
    const float tt = limitar((t - 0.45f) / 0.45f, 0.f, 1.f);
    if (tt <= 0.f)
        return;
    Pen traco(comAlfa(cor, 1.f), 6.f);
    traco.SetStartCap(LineCapRound);
    traco.SetEndCap(LineCapRound);
    traco.SetLineJoin(LineJoinRound);
    if (!erro) {
        const PointF a(cx - 20, cy + 2), b(cx - 6, cy + 16), c(cx + 22, cy - 14);
        const float parte1 = limitar(tt * 2.f, 0.f, 1.f), parte2 = limitar(tt * 2.f - 1.f, 0.f, 1.f);
        g.DrawLine(&traco, a, PointF(a.X + (b.X - a.X) * parte1, a.Y + (b.Y - a.Y) * parte1));
        if (parte2 > 0.f)
            g.DrawLine(&traco, b, PointF(b.X + (c.X - b.X) * parte2, b.Y + (c.Y - b.Y) * parte2));
    } else {
        const float p = suavizar(tt);
        g.DrawLine(&traco, cx, cy - 20, cx, cy - 20 + 26 * p);
        if (p > 0.9f) {
            SolidBrush ponto(cor);
            g.FillEllipse(&ponto, cx - 3.5f, cy + 14, 7.f, 7.f);
        }
    }
}

void Tela::desenhar(Graphics &g, double t)
{
    fundo(g, t);
    const float local = static_cast<float>(t - inicioDaPagina_);
    const float entrada = suavizar(local / 0.55f);   // 0..1 na entrada da página
    const float alfa = entrada;
    const float desloc = (1.f - entrada) * 18.f;
    const float cx = kLargura / 2;

    auto linhaDeProgresso = [&](const std::wstring &titulo1, const std::wstring &subtitulo1) {
        logo(g, cx, 112, 96, t);
        titulo(g, titulo1, 182, alfa, desloc);
        subtitulo(g, subtitulo1, 238, alfa, desloc, 30.f);
        const float x = cx - 270, w = 540;
        barraDeProgresso(g, x, 318, w, progressoSuave_, t);
        const int pct = static_cast<int>(std::lround(progressoSuave_ * 100.f));
        texto(g, std::to_wstring(pct) + L"%", *fSubtitulo_, RectF(x + w - 80, 288, 80, 24), comAlfa(kTinta, alfa), StringAlignmentFar, StringAlignmentCenter, true);
        texto(g, arquivoAtual(), *fPequena_, RectF(x, 340, w, 20), comAlfa(kTintaSuave, alfa), StringAlignmentNear, StringAlignmentNear, true, true);
    };

    switch (pagina_) {
    case Pagina::BoasVindas: {
        logo(g, cx, 100, 104, t);
        titulo(g, config_.jaInstalado ? L"Atualizar o Caderno+" : L"Bem-vindo ao Caderno+", 168, alfa, desloc);
        std::wstring sub = L"Turmas, notas, frequência, aulas e um assistente de IA num só lugar. Tudo no seu computador, sem precisar de internet.";
        if (config_.jaInstalado)
            sub = L"A versão " + (config_.versaoInstalada.empty() ? std::wstring(L"instalada") : config_.versaoInstalada) +
                  L" será atualizada para a " + config_.versao + L". Seus dados são mantidos.";
        subtitulo(g, sub, 224, alfa, desloc, 56.f);
        cartaoDeOpcoes(g, alfa, desloc);
        break;
    }
    case Pagina::Instalando:
        linhaDeProgresso(L"Instalando…", [&] {
            static const wchar_t *dicas[] = {L"Ctrl+K busca em tudo: turmas, alunos, aulas e anotações.",
                                             L"O backup automático guarda seus dados todo dia.",
                                             L"Funciona sem internet: seus dados ficam só neste computador.",
                                             L"Importe a lista de alunos de um Excel em poucos cliques.",
                                             L"O painel Hoje mostra aulas, tarefas e alunos que pedem atenção."};
            return std::wstring(dicas[static_cast<size_t>(local / 4.5f) % 5]);
        }());
        break;
    case Pagina::Removendo:
        linhaDeProgresso(L"Removendo…", L"Seus dados serão mantidos, a menos que você tenha pedido para apagá-los.");
        break;
    case Pagina::Concluido: {
        marcaDeConcluido(g, cx, 112, local, kPrimaria, false);
        titulo(g, L"Tudo pronto!", 180, alfa, desloc);
        subtitulo(g, L"O Caderno+ foi instalado. Procure por \"Caderno+\" no Menu Iniciar quando quiser abrir.", 236, alfa, desloc, 56.f);
        break;
    }
    case Pagina::Removido:
        marcaDeConcluido(g, cx, 112, local, kPrimaria, false);
        titulo(g, L"Caderno+ removido", 180, alfa, desloc);
        subtitulo(g, apagarDados_ ? L"O programa e os seus dados foram removidos deste computador."
                                  : L"O programa foi removido. Seus dados continuam guardados; se reinstalar, tudo estará lá.",
                  236, alfa, desloc, 56.f);
        break;
    case Pagina::Erro:
        marcaDeConcluido(g, cx, 100, local, kPerigo, true);
        titulo(g, L"Não foi possível concluir", 166, alfa, desloc);
        texto(g, erro_, *fSubtitulo_, RectF(110, 224 + desloc, kLargura - 220, 180), comAlfa(kTintaSuave, alfa), StringAlignmentCenter, StringAlignmentNear);
        break;
    case Pagina::Confirmar:
        logo(g, cx, 100, 100, t);
        titulo(g, L"Remover o Caderno+?", 168, alfa, desloc);
        subtitulo(g, L"O programa será removido deste computador. Por padrão, as suas turmas, notas e contas ficam guardadas.", 224, alfa, desloc, 56.f);
        break;
    }

    for (size_t i = 0; i < controles_.size(); ++i)
        controle(g, controles_[i], teclado_ && static_cast<int>(i) == foco_ && !ocupado_, alfa, desloc);

    // Rodapé
    if (pagina_ == Pagina::BoasVindas) {
        std::wstring rodape = L"Versão " + config_.versao;
        if (config_.tamanhoEmMB)
            rodape += L"  ·  " + std::to_wstring(config_.tamanhoEmMB) + L" MB";
        rodape += L"  ·  Não precisa de administrador";
        texto(g, rodape, *fPequena_, RectF(0, kAltura - 34, kLargura, 20), comAlfa(kTintaSuave, 0.8f * alfa), StringAlignmentCenter, StringAlignmentCenter, true);
        if (config_.aplicativoAberto)
            texto(g, L"O Caderno+ está aberto e será fechado durante a instalação.", *fPequena_, RectF(0, 410, kLargura, 20), comAlfa(kDestaque, alfa),
                  StringAlignmentCenter, StringAlignmentCenter, true);
    }
    // Marca no canto superior esquerdo
    {
        const float w = largura(g, L"Caderno", *fNegrito_);
        StringFormat sf(StringFormat::GenericTypographic());
        sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsNoWrap);
        SolidBrush tinta(comAlfa(kTinta, 0.9f)), mais(kDestaque);
        g.DrawString(L"Caderno", -1, fNegrito_.get(), PointF(24, 21), &sf, &tinta);
        g.DrawString(L"+", -1, fNegrito_.get(), PointF(24 + w + 0.5f, 21), &sf, &mais);
    }
    botoesDaJanela(g);
}

}  // namespace

// -------------------------------------------------------------------- API

int executar(HINSTANCE instancia, const Configuracao &config)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Tela tela(instancia, config);
    if (!tela.criarJanela(SW_SHOWNORMAL))
        return 1;
    const int codigo = tela.laco();
    CoUninitialize();
    return codigo;
}

static bool salvarPng(Bitmap &imagem, const std::wstring &arquivo)
{
    UINT n = 0, tamanho = 0;
    GetImageEncodersSize(&n, &tamanho);
    if (tamanho == 0)
        return false;
    std::vector<BYTE> bruto(tamanho);
    auto *codificadores = reinterpret_cast<ImageCodecInfo *>(bruto.data());
    GetImageEncoders(n, tamanho, codificadores);
    for (UINT i = 0; i < n; ++i)
        if (wcscmp(codificadores[i].MimeType, L"image/png") == 0)
            return imagem.Save(arquivo.c_str(), &codificadores[i].Clsid, nullptr) == Ok;
    return false;
}

bool capturar(HINSTANCE instancia, const Configuracao &config, const std::wstring &pasta)
{
    Sistema::criarPastas(pasta);
    Tela tela(instancia, config);
    struct Quadro {
        Pagina pagina;
        const wchar_t *nome;
        int milesimos;
        const wchar_t *arquivo;
    };
    const std::vector<Quadro> quadros = {
        {config.modo == Modo::Instalar ? Pagina::BoasVindas : Pagina::Confirmar, L"1-inicio", 0, L""},
        {config.modo == Modo::Instalar ? Pagina::Instalando : Pagina::Removendo, L"2-progresso", 640, L"Qt6Widgets.dll"},
        {config.modo == Modo::Instalar ? Pagina::Concluido : Pagina::Removido, L"3-concluido", 1000, L""},
        {Pagina::Erro, L"4-erro", 0, L""},
    };
    bool ok = true;
    for (const Quadro &q : quadros) {
        tela.definirPagina(q.pagina);
        tela.simularAndamento(q.milesimos, q.arquivo);
        tela.definirErroSimulado(L"Não foi possível gravar \"Qt6Core.dll\". O disco pode estar cheio ou a pasta protegida.");
        Bitmap imagem(static_cast<int>(kLargura), static_cast<int>(kAltura), PixelFormat32bppPARGB);
        {
            Graphics g(&imagem);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintAntiAlias);
            g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            // O momento do desenho é "um pouco depois" da entrada da página: tudo já apareceu e a barra está no valor.
            tela.desenhar(g, agora() + 1.2);
        }
        ok = salvarPng(imagem, Sistema::juntar(pasta, std::wstring(q.nome) + L".png")) && ok;
    }
    return ok;
}

}  // namespace Janela

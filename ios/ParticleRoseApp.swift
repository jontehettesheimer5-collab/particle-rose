import SwiftUI
import WebKit

@main
struct ParticleRoseApp: App {
    var body: some Scene {
        WindowGroup {
            RoseView()
                .ignoresSafeArea()
                .background(Color.black)
                .statusBarHidden(true)
                .persistentSystemOverlays(.hidden)
        }
    }
}

struct RoseView: UIViewRepresentable {
    func makeUIView(context: Context) -> WKWebView {
        let webView = WKWebView()
        webView.isOpaque = false
        webView.backgroundColor = .black
        webView.scrollView.backgroundColor = .black
        webView.scrollView.isScrollEnabled = false
        webView.scrollView.contentInsetAdjustmentBehavior = .never
        if let url = Bundle.main.url(forResource: "index", withExtension: "html", subdirectory: "web") {
            webView.loadFileURL(url, allowingReadAccessTo: url.deletingLastPathComponent())
        }
        return webView
    }

    func updateUIView(_ uiView: WKWebView, context: Context) {}
}

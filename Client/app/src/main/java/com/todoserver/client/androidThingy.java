//How MANY MORE FUCKKING LANGUAGES THAT I NEED FOR THE FUCKING THINGGGGGGGGGGG

package com.todoserver.client;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.graphics.Insets;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.view.WindowInsets;
import android.view.View;
import android.webkit.CookieManager;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceError;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.FrameLayout;
import android.widget.Toast;
import android.widget.TextView;
import android.window.OnBackInvokedDispatcher;

public final class androidThingy extends Activity {
    private WebView webView;
    private View errorPage;
    private String failedUrl;
    private boolean connectionError;
    private final Uri server = Uri.parse(BuildConfig.SERVER_URL);

    @SuppressLint("SetJavaScriptEnabled")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        FrameLayout container = new FrameLayout(this);
        webView = new WebView(this);
        container.addView(webView, new FrameLayout.LayoutParams(-1, -1));
        errorPage = getLayoutInflater().inflate(R.layout.connection_error, container, false);
        container.addView(errorPage);
        errorPage.findViewById(R.id.retry).setOnClickListener(view -> {
            String retryUrl = failedUrl;
            if (retryUrl != null) webView.loadUrl(retryUrl);
        });
        setContentView(container);

        // Keep the webpage above system bars, display cutouts, and the keyboard.
        if (Build.VERSION.SDK_INT >= 30) {
            getWindow().setDecorFitsSystemWindows(false);
            container.setOnApplyWindowInsetsListener((view, windowInsets) -> {
                Insets insets = windowInsets.getInsets(WindowInsets.Type.systemBars()
                        | WindowInsets.Type.displayCutout() | WindowInsets.Type.ime());
                view.setPadding(insets.left, insets.top, insets.right, insets.bottom);
                return windowInsets;
            });
            container.requestApplyInsets();
        }

        WebSettings settings = webView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        // The frontend waits for Play; allow its subsequent queued track transitions.
        settings.setMediaPlaybackRequiresUserGesture(false);
        settings.setAllowFileAccess(false);
        settings.setAllowContentAccess(false);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
        settings.setUseWideViewPort(true);
        settings.setLoadWithOverviewMode(true);
        CookieManager.getInstance().setAcceptCookie(true);
        CookieManager.getInstance().setAcceptThirdPartyCookies(webView, false);
        WebView.setWebContentsDebuggingEnabled(BuildConfig.DEBUG);
        webView.setWebChromeClient(new WebChromeClient());
        webView.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageStarted(WebView view, String url, Bitmap favicon) {
                failedUrl = null;
                errorPage.setVisibility(View.GONE);
                webView.setVisibility(View.VISIBLE);
            }

            @Override
            public void onReceivedError(WebView view, WebResourceRequest request,
                    WebResourceError error) {
                if (!request.isForMainFrame() || !isServerOrigin(request.getUrl())) return;
                int code = error.getErrorCode();
                if (code == ERROR_HOST_LOOKUP || code == ERROR_CONNECT
                        || code == ERROR_TIMEOUT || code == ERROR_IO) {
                    showError(request.getUrl().toString(), true);
                }
            }

            @Override
            public void onReceivedHttpError(WebView view, WebResourceRequest request,
                    WebResourceResponse response) {
                // Cloudflare's 1033 is in the response body, with HTTP status 530.
                // Other server/tunnel failures also belong on the unavailable screen.
                if (request.isForMainFrame() && isServerOrigin(request.getUrl())
                        && response.getStatusCode() >= 500) {
                    showError(request.getUrl().toString(), false);
                }
            }

            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                // Embedded shit remains in the page; only top level shit gets to leave
                if (!request.isForMainFrame()) return false;
                
                Uri destination = request.getUrl();
                
                if (isServerOrigin(destination)) return false;
                
                String scheme = destination.getScheme();
                
                if ("https".equalsIgnoreCase(scheme) || "http".equalsIgnoreCase(scheme)) {
                    try {
                        startActivity(new Intent(Intent.ACTION_VIEW, destination)
                                .addCategory(Intent.CATEGORY_BROWSABLE));
                    } catch (ActivityNotFoundException exception) {
                        Toast.makeText(androidThingy.this, R.string.no_browser, Toast.LENGTH_SHORT).show();
                    }
                }
                return true;
            }
        });

        if (Build.VERSION.SDK_INT >= 33) {
            getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                    OnBackInvokedDispatcher.PRIORITY_DEFAULT, this::navigateBack);
        }
        
        if (savedInstanceState == null || webView.restoreState(savedInstanceState) == null) {
            webView.loadUrl(BuildConfig.SERVER_URL);
        } else if (savedInstanceState.getString("failedUrl") != null) {
            showError(savedInstanceState.getString("failedUrl"),
                    savedInstanceState.getBoolean("connectionError"));
        }
    }

    private void showError(String url, boolean isConnectionError) {
        failedUrl = url;
        connectionError = isConnectionError;
        ((TextView) errorPage.findViewById(R.id.error_title)).setText(isConnectionError
                ? R.string.connection_error_title : R.string.server_error_title);
        ((TextView) errorPage.findViewById(R.id.error_message)).setText(isConnectionError
                ? R.string.connection_error_message : R.string.server_error_message);
        webView.setVisibility(View.GONE);
        errorPage.setVisibility(View.VISIBLE);
    }

    private boolean isServerOrigin(Uri uri) {

        return "https".equalsIgnoreCase(uri.getScheme())
                && server.getHost().equalsIgnoreCase(uri.getHost())
                && (server.getPort() == -1 ? 443 : server.getPort())
                == (uri.getPort() == -1 ? 443 : uri.getPort());
    }

    private void navigateBack() {

        if (webView.canGoBack()) webView.goBack();
        else finish();
    }

    @SuppressLint("GestureBackNavigation")
    @SuppressWarnings("deprecation")
    @Override
    public void onBackPressed() {
        navigateBack();
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        webView.saveState(outState);
        outState.putString("failedUrl", failedUrl);
        outState.putBoolean("connectionError", connectionError);
        super.onSaveInstanceState(outState);
    }

    @Override
    protected void onPause() {
        webView.onPause();
        CookieManager.getInstance().flush();
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        webView.onResume();
    }

    @Override
    protected void onDestroy() {
        ((FrameLayout) webView.getParent()).removeView(webView);
        webView.destroy();
        super.onDestroy();
    }
}

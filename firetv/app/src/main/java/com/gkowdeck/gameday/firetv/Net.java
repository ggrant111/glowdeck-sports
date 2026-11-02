package com.gkowdeck.gameday.firetv;

import org.json.JSONObject;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;

/** Network calls must always be made off the Android main thread. */
final class Net {
    static JSONObject get(String url) throws Exception { return request("GET", url, null); }
    static JSONObject post(String url, JSONObject body) throws Exception { return request("POST", url, body); }
    private static JSONObject request(String method, String url, JSONObject body) throws Exception {
        HttpURLConnection conn = (HttpURLConnection)new URL(url).openConnection();
        try {
            conn.setConnectTimeout(4500);
            conn.setReadTimeout(8500);
            conn.setRequestMethod(method);
            conn.setRequestProperty("Accept","application/json");
            conn.setRequestProperty("User-Agent","GlowDeckGameDay-FireTV/0.1");
            if (body != null) {
                conn.setDoOutput(true);
                conn.setRequestProperty("Content-Type","application/json; charset=utf-8");
                byte[] bytes = body.toString().getBytes(StandardCharsets.UTF_8);
                if (bytes.length > 8192) throw new Exception("WLED request too large");
                try (OutputStream out = conn.getOutputStream()) { out.write(bytes); }
            }
            int code = conn.getResponseCode();
            InputStream stream = (code >= 200 && code < 300) ? conn.getInputStream() : conn.getErrorStream();
            byte[] data = readBounded(stream,1024*1024);
            if (code < 200 || code >= 300) throw new Exception("HTTP "+code+" from "+conn.getURL().getHost());
            if (data.length == 0) return new JSONObject();
            return new JSONObject(new String(data,StandardCharsets.UTF_8));
        } finally { conn.disconnect(); }
    }
    private static byte[] readBounded(InputStream in, int limit) throws Exception {
        if (in == null) return new byte[0];
        try (InputStream stream=in; ByteArrayOutputStream out=new ByteArrayOutputStream()) {
            byte[] buffer=new byte[4096];int n,total=0;
            while ((n=stream.read(buffer))!=-1) {
                total+=n;if(total>limit)throw new Exception("Response exceeds 1 MB safety limit");
                out.write(buffer,0,n);
            }
            return out.toByteArray();
        }
    }
}

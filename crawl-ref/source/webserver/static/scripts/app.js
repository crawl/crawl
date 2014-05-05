require.config({
    baseUrl: "/static/scripts",
    paths: {
        "jquery": use_cdn ? "//cdnjs.cloudflare.com/ajax/libs/jquery/1.8.2/jquery.min" : "/static/scripts/contrib/jquery",
        "react": use_cdn ? "//cdnjs.cloudflare.com/ajax/libs/react/0.10.0/react.min" : "/static/scripts/contrib/react-0.10.0",
        "text": "/static/scripts/contrib/text",
        "jsx": "/static/scripts/contrib/jsx",
        "JSXTransformer": "/static/scripts/contrib/JSXTransformer-0.10.0",
        "image": "/static/scripts/contrib/image"
    },
    jsx: {
        fileExtension: ".jsx"
    }
});

require(["client"]);

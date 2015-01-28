require.config({
    baseUrl: "/static/scripts",
    paths: {
        "jquery": use_cdn ? "//cdnjs.cloudflare.com/ajax/libs/jquery/1.11.2/jquery.min" : "/static/scripts/contrib/jquery",
        "react": use_cdn ? "//cdnjs.cloudflare.com/ajax/libs/react/0.12.2/react.min" : "/static/scripts/contrib/react",
        "text": "/static/scripts/contrib/text",
        "jsx": "/static/scripts/contrib/jsx",
        "JSXTransformer": "/static/scripts/contrib/JSXTransformer",
        "react-bootstrap": "/static/scripts/contrib/react-bootstrap/react-bootstrap",
        "image": "/static/scripts/contrib/image"
    },
    jsx: {
        fileExtension: ".jsx"
    }
});

require(["client"]);

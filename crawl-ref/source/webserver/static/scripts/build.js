({
    baseUrl: '.',
    paths: {
        'jquery': 'empty:',
        'react': './contrib/react-0.10.0',
        'text': './contrib/text',
        'jsx': './contrib/jsx',
        'JSXTransformer': './contrib/JSXTransformer-0.10.0',
        "image": "./contrib/image"
    },
    jsx: {
        fileExtension: '.jsx'
    },

    optimize: "uglify2",

    exclude: ["react", "jsx"],

    name: 'app',
    out: 'app.min.js',

    onBuildWrite: function (moduleName, path, singleContents) {
        return singleContents.replace(/jsx!/g, '');
    }
})

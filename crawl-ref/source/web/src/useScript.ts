import { useEffect } from "react";

const useScript = (url: string, loadCondition: boolean) => {
  useEffect(() => {
    if (!loadCondition) {
      return;
    }
    const script = document.createElement("script");

    script.async = true;
    script.src = url;

    document.body.appendChild(script);

    return () => {
      document.body.removeChild(script);
    };
  }, [url, loadCondition]);
};

export default useScript;

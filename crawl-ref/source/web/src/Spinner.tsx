import styled from "@emotion/styled";

export const Spinner = styled.div`
  position: absolute;
  top: 0;
  right: 0;
  height: 30px;
  width: 30px;
  margin: 0;
  margin-top: 20px;
  margin-left: 20px;
  display: inline-block;
  vertical-align: top;

  -webkit-animation: rotation 0.8s linear infinite;
  -moz-animation: rotation 0.8s linear infinite;
  -o-animation: rotation 0.8s linear infinite;
  animation: rotation 0.8s linear infinite;

  border-left: 5px solid rgb(235, 235, 235);
  border-right: 5px solid rgb(235, 235, 235);
  border-bottom: 5px solid rgb(235, 235, 235);
  border-top: 5px solid rgb(120, 120, 120);

  border-radius: 100%;
  background-color: rgb(189, 215, 46);

  @-webkit-keyframes rotation {
    from {
      -webkit-transform: rotate(0deg);
    }
    to {
      -webkit-transform: rotate(360deg);
    }
  }
  @-moz-keyframes rotation {
    from {
      -moz-transform: rotate(0deg);
    }
    to {
      -moz-transform: rotate(360deg);
    }
  }
  @-o-keyframes rotation {
    from {
      -o-transform: rotate(0deg);
    }
    to {
      -o-transform: rotate(360deg);
    }
  }
  @keyframes rotation {
    from {
      transform: rotate(0deg);
    }
    to {
      transform: rotate(360deg);
    }
  }
`;

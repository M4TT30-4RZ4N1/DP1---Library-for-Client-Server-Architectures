function greatest() {

  
  var max=arguments[0];
	
  for(var i=0;i< arguments.length; i++){

	if(arguments[i]>=max){
		max=arguments[i];
	}
  }

  return max;

}

function passparameter(){

  var numero1 = parseFloat(document.getElementById("numero1").value);
  var numero2 = parseFloat(document.getElementById("numero2").value);
  var numero3 = parseFloat(document.getElementById("numero3").value);
  var numero4 = parseFloat(document.getElementById("numero4").value);
  var numero5 = parseFloat(document.getElementById("numero5").value);

  var max= greatest(numero1,numero2,numero3,numero4,numero5);

  alert("Il numero più grande è: " + max);

}

function cancella() {
  document.getElementById("numero1").value = "";
  document.getElementById("numero2").value = "";
  document.getElementById("numero3").value = "";
  document.getElementById("numero4").value = "";
  document.getElementById("numero5").value = "";
}

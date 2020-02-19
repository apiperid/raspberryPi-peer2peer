/*
   uint32_t getFirstHalfAEM(uint32_t AEM)

   get the xx part of the AEM

*/
uint32_t getFirstHalfAEM(uint32_t AEM)
{
  return (uint32_t)(AEM/100);
}

/*
   uint32_t getSecondHalfAEM(uint32_t AEM)

   get the yy part of the AEM

*/

uint32_t getSecondHalfAEM(uint32_t AEM)
{
  return (uint32_t)(AEM%100);
}
